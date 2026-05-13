// SQBinding - C++11 binding for Squirrel
// Copyright (c) 2025 Ligverd
// Licensed under the MIT License. See LICENSE file for details.

#ifndef SQB_CLASS_H
#define SQB_CLASS_H

#include <stdexcept>
#include <string>
#include <memory>
#include "sqbdetail.h"
#include "sqbobject.h"

namespace sqb {


struct SmartBase {
  detail::ExtractFunc pack    = nullptr;
  detail::ExtractFunc extract = nullptr;
  SQRELEASEHOOK releaseHook   = nullptr;
  bool is_smart = false;

  size_t typetagSmart = 0;


  SmartBase() {
    pack     = [](void* c) -> void* { return c; };
    extract  = [](void* p) -> void* { return p; };
    is_smart = false;
  }
};

template<template<typename> class T>
struct Smart : public SmartBase {
  template<typename ClassType>
  struct Rebind {
    using Container = T<ClassType>;
  };

  template<typename ClassType, typename BaseClassType = void>
  static SmartBase prepare() {
    SmartBase sb;
    using Container = T<ClassType>;

    sb.pack        = [](void* c) -> void* { return new Container(static_cast<ClassType*>(c)); };
    sb.extract     = [](void* p) -> void* { return static_cast<Container*>(p)->get(); };
    sb.releaseHook = &types::release_hook_delete<Container>;
    sb.is_smart    = true;
    sb.typetagSmart= typeid(Container).hash_code();

    using BaseClassIsVoid = typename std::is_same<BaseClassType, void>::type;
    if (BaseClassIsVoid::value)
      types::Type::create<T<ClassType>>(OT_INSTANCE);
    else
      types::Type::create<T<ClassType>, T<BaseClassType>>(OT_INSTANCE);

    return sb;
  }
};


template<typename ClassType, typename BaseClassType = void>
class SQBClass : public SQBObject
{
protected:
  struct SmartStrategy {
    void* (*packFunc)(ClassType*)   = nullptr;
    detail::ExtractFunc extractFunc = nullptr;
    SQRELEASEHOOK releaseHook       = nullptr;

    size_t      thisTypetagOriginal  = 0;
    size_t      thisTypetagTarget    = 0;

    const detail::MethodDescriptor md(bool isStatic=false) const
    {
      auto ext = isStatic ? nullptr : extractFunc;
      return {ext, thisTypetagOriginal, thisTypetagTarget, isStatic};
    }
  };

  SmartStrategy _strategy;

public:

  // only get interface SQBObject from vm
  SQBClass(HSQUIRRELVM vm, HSQOBJECT obj)
      : SQBObject(vm, obj)
  {
    if (hsqObject._type != OT_CLASS)
      throw std::runtime_error("HSQOBJECT not OT_CLASS");
  }


  SQBClass(const SQBObject &parent, const std::string &name, SmartBase sb = SmartBase())
      : SQBObject(parent.vm)
  {
    if (name.empty())
      throw std::runtime_error("Failed create class with empty name");

    setParent(parent);
    int top = parent.push();

    sq_pushstring(vm, name.c_str(), -1);

    using BaseClassIsVoid = typename std::is_same<BaseClassType, void>::type;

    // Register default types
    types::Type::create<ClassType,  ClassType*>(OT_INSTANCE, name);
    types::Type::create<ClassType*, ClassType>(OT_INSTANCE);

    if (BaseClassIsVoid::value) {
      sq_newclass(vm, false); // [table, string] without inheritance
    }else{
      const types::Type &t = types::Type::get<BaseClassType>();
      if (t.type == 0 )
        throw std::runtime_error("BaseClassType not registered yet");

      types::Type::create<ClassType,  ClassType*, BaseClassType, BaseClassType*>(OT_INSTANCE);
      types::Type::create<ClassType*, ClassType,  BaseClassType, BaseClassType*>(OT_INSTANCE);

      SQBClass cl(vm, parent.find(t.name));
      cl.push();
      sq_newclass(vm, true);  // [table, string, class]
    }

    size_t typetag = sb.typetagSmart;
    if (typetag == 0) {
      typetag  = typeid(ClassType).hash_code();
      _strategy.thisTypetagOriginal = typetag;
      _strategy.thisTypetagTarget   = typetag;
    }else{
      _strategy.thisTypetagOriginal = typetag;
      _strategy.thisTypetagTarget   = typeid(ClassType).hash_code();
    }

    sq_settypetag(vm, -1, reinterpret_cast<SQUserPointer>(typetag));

    sq_getstackobj(vm, -1, &hsqObject);
    sq_addref(vm, &hsqObject);

    if(SQ_FAILED(sq_newslot(vm, -3, SQFalse))) {
      sq_settop(vm, 0);
      throw std::runtime_error("Failed to register class");
    }

    needRelease = true;
    pop(top);

    _getterMap = detail::initPropHook<ClassType>(this, false);
    _setterMap = detail::initPropHook<ClassType>(this, true);

    _strategy.packFunc    = reinterpret_cast<void*(*)(ClassType*)>(sb.pack);
    _strategy.extractFunc = sb.extract;
    _strategy.releaseHook = sb.releaseHook ? sb.releaseHook : &types::release_hook_delete<ClassType>;
  }

  template <typename... Args>
  SQBClass& bindConstructor() {
    //auto func = [](Args... args){ return new ClassType(args...); };
    //instanceAllocator(func);
    HSQUIRRELVM v = vm;
    SmartStrategy h = _strategy;

    detail::registerFunction(this, "constructor", [v, h](Args... args) -> void {
      auto obj = h.packFunc( new ClassType(args...) );
      sq_setinstanceup(v, 1, obj );
      sq_setreleasehook(v, 1, h.releaseHook);
      sq_pop(v, 2);
    });
    return *this;
  }

  template <typename Func>
  SQBClass& bindConstructor(const Func lambda) {
    auto func = detail::make_function(lambda);
    instanceAllocator(func);
    return *this;
  }


  template <typename Ret, typename... Args>
  SQBClass& bindMethod(const char* name, Ret (ClassType::*method)(Args...))
  {
    detail::registerFunction(this, name, [method](ClassType *self, Args... args) -> Ret {
      return (self->*method)(args...);
    }, _strategy.md());
    return *this;
  }

  template <typename Func>
  SQBClass& bindMethod(const std::string &name, Func func) {
    detail::registerFunction(this, name, func, _strategy.md());
    return *this;
  }

  template <typename Ret, typename... Args>
  SQBClass& bindStaticMethod(const char* name, Ret (ClassType::*method)(Args...))
  {
    detail::registerFunction(this, name, [method](Args... args) -> Ret {
      return (*method)(args...);
    }, _strategy.md(true)); //nullptr, true);
    return *this;
  }

  template <typename Func>
  SQBClass& bindStaticMethod(const std::string &name, Func func) {
    detail::registerFunction(this, name, func, _strategy.md(true));
    return *this;
  }


  /*
    getter
    --- stack top 3 ---
    -3 OT_INSTANCE    this
    -2 OT_STRING      key
    -1 OT_USERPOINTER map

    setter
    --- stack top 4 ---
    -4 OT_INSTANCE    this
    -3 OT_STRING      key
    -2 OT_STRING      value
    -1 OT_USERPOINTER map
  */
  template<typename PropType>
  SQBClass& bindProp(const std::string &name, PropType ClassType::* prop, bool readOnly = false) {
    auto strategy = _strategy;

    (*_getterMap)[name] = [prop, strategy](HSQUIRRELVM vm) {
      SQUserPointer up = types::popValuePointer(vm, -3);
      ClassType *c = static_cast<ClassType*>( strategy.extractFunc(up) );
      types::pushValue<PropType>(vm, c->*prop);
      return 1;
    };

    (*_setterMap)[name] = [prop, name, readOnly, strategy](HSQUIRRELVM vm) {
      if (readOnly) {
        error(vm, "property '%s' read only!", name);
      }else{
        SQUserPointer up = types::popValuePointer(vm, -3);
        ClassType *c = static_cast<ClassType*>( strategy.extractFunc(up) );
        c->*prop = types::popValue<PropType>(vm, -2);
      }
      return 0;
    };

    return *this;
  }

  template<typename PropType>
  SQBClass& bindProp(const std::string &name, std::function<PropType(ClassType*)> getter, std::function<void(ClassType*, PropType)> setter = nullptr) {
    auto strategy = _strategy;

    if (getter) {
      (*_getterMap)[name] = [getter, strategy](HSQUIRRELVM vm) {
        SQUserPointer up = types::popValuePointer(vm, -3);
        ClassType *c = strategy.extractInstance(up);
        types::pushValue<PropType>(vm, getter(c));
        return 1;
      };
    }

    if (setter) {
      (*_setterMap)[name] = [setter, strategy](HSQUIRRELVM vm) {
        SQUserPointer up = types::popValuePointer(vm, -4);
        ClassType *c = strategy.extractInstance(up);
        setter(c, types::popValue<PropType>(vm, -2));
        return 0;
      };
    }

    return *this;
  }

  /*
  template<typename PropType>
  SQBClass& bindStaticProp(const std::string &name, PropType *prop, bool readOnly = false) {

    Scurrel not support this

    use bindStaticMethod(name, getter, setter)

    Class.setProp(value)
    value = Class.getProp()

    fuck :(

    return *this;
  }
  */

private:
  template <typename Ret, typename... Args, typename std::enable_if<!std::is_pointer<Ret>::value, Ret>::type* = nullptr>
  void instanceAllocator(const std::function<Ret(Args...)> func)
  {
    HSQUIRRELVM v = vm;
    SmartStrategy h = _strategy;

    detail::registerFunction(this, "constructor", [v, h, func](Args... args) -> void {
      auto data = h.packFunc(new ClassType( func(args...) ));
      sq_setinstanceup(v, 1, data);
      sq_setreleasehook(v, 1, h.releaseHook);
    });
  }

  template <typename Ret, typename... Args, typename std::enable_if<std::is_pointer<Ret>::value, Ret>::type* = nullptr>
  void instanceAllocator(const std::function<Ret(Args...)> func)
  {
    HSQUIRRELVM v = vm;
    SmartStrategy h = _strategy;
    detail::registerFunction(this, "constructor", [v, h, func](Args... args) -> void {
      auto obj = h.packFunc( func(args...) );
      sq_setinstanceup(v, 1, obj);
      sq_setreleasehook(v, 1, h.releaseHook);
    });
  }


  detail::FunctionMap *_setterMap;
  detail::FunctionMap *_getterMap;



  std::function<size_t(size_t(*)( ))> get_base_hash;


  // clac smart base type
  struct IHashLink {
    virtual size_t getDerivedHash() = 0;
    virtual size_t getBaseHash() = 0;
    virtual ~IHashLink() = default;
  };

  template<template<typename> class T, typename D, typename B>
  struct HashLinkImpl : public IHashLink {
    size_t getDerivedHash() override { return typeid(T<D>).hash_code(); }
    size_t getBaseHash()    override { return typeid(T<B>).hash_code(); }
  };

  std::function<std::unique_ptr<IHashLink>(size_t)> link_factory;

};

}
#endif
