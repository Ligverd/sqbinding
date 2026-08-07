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

// smart type carrier
template<template<typename> class T>
struct Smart {};


struct SmartBase {
  detail::ExtractFunc pack    = nullptr;
  detail::ExtractFunc extract = nullptr;
  SQRELEASEHOOK releaseHook   = nullptr;
  bool is_smart = false;

  size_t thisTypetagOriginal  = 0;
  size_t thisTypetagTarget    = 0;

  SmartBase()
  {
    pack     = [](void* c) -> void* { return c; };
    extract  = [](void* p) -> void* { return p; };
    is_smart = false;
  }

  const detail::MethodDescriptor md(bool isStatic=false, size_t numberOptionalArguments=0) const
  {
    if (thisTypetagOriginal == 0 && thisTypetagTarget == 0)
      throw std::runtime_error("Smart not prepared!");

    auto ext = isStatic ? nullptr : extract;
    return {ext, thisTypetagOriginal, thisTypetagTarget, isStatic, numberOptionalArguments};
  }

  template<typename ClassType, typename BaseClassType = void>
  static SmartBase create() {
    auto &smart_map = getSmartMap();

    using isVoidBaseClassType = typename std::is_same<BaseClassType, void>::type;

    if (!isVoidBaseClassType::value) {
      auto it = smart_map.find(typeid(BaseClassType).hash_code());
      if (it != smart_map.end()) {
        SmartBase s = it->second;
        if (s.is_smart)
          throw std::runtime_error( "class " + types::name<ClassType>() + " need must be Smart as " + types::name<BaseClassType>() );
      }
    }

    SmartBase sb;

    sb.is_smart            = false;
    sb.thisTypetagOriginal = typeid(ClassType).hash_code();
    sb.thisTypetagTarget   = sb.thisTypetagOriginal;
    sb.releaseHook         = &types::release_hook_delete<ClassType>;

    smart_map[ typeid(ClassType).hash_code() ] = sb;

    return sb;
  }


  template<typename ClassType, template<typename> class T>
  static SmartBase create(Smart<T> s) {
    return create<ClassType,void>(s);
  }


  template<typename ClassType, typename BaseClassType, template<typename> class T>
  static SmartBase create(Smart<T>) {
    auto &smart_map = getSmartMap();

    using isVoidBaseClassType = typename std::is_same<BaseClassType, void>::type;

    if (isVoidBaseClassType::value) {
      types::Type::create<T<ClassType>>(OT_INSTANCE);
    }else{
      auto it = smart_map.find(typeid(BaseClassType).hash_code());
      if (it != smart_map.end()) {
        SmartBase s = it->second;
        if (!s.is_smart)
          throw std::runtime_error( "class " + types::name<ClassType>() + " cannot be smart because " + types::name<BaseClassType>() + " is not Smart");
        if ( s.thisTypetagOriginal != typeid(T<BaseClassType>).hash_code() )
          throw std::runtime_error("The Smart type must match the " + types::name<BaseClassType>() + " Smart type.");
      }

      types::Type::create<T<ClassType>, T<BaseClassType>>(OT_INSTANCE);
    }

    using Container = T<ClassType>;
    SmartBase sb;

    sb.pack        = [](void* c) -> void* { return new Container(static_cast<ClassType*>(c)); };
    sb.extract     = [](void* p) -> void* { return static_cast<Container*>(p)->get(); };

    sb.is_smart    = true;
    sb.thisTypetagOriginal = typeid(Container).hash_code();
    sb.thisTypetagTarget   = typeid(ClassType).hash_code();
    sb.releaseHook = &types::release_hook_delete<Container>;

    smart_map[ typeid(ClassType).hash_code() ] = sb;

    return sb;
  }

protected:
  static std::unordered_map<size_t,SmartBase>& getSmartMap()
  {
    static std::unordered_map<size_t, SmartBase> smart_map;
    return smart_map;
  }
};




template<typename ClassType, typename BaseClassType = void>
class SQBClass : public SQBObject
{
protected:
  SmartBase _strategy;

public:

  // only get interface SQBObject from vm
  SQBClass(HSQUIRRELVM vm, HSQOBJECT obj)
      : SQBObject(vm, obj)
  {
    if (hsqObject._type != OT_CLASS)
      throw std::runtime_error("HSQOBJECT not OT_CLASS");
  }


  SQBClass(const SQBObject &parent, const std::string &name, SmartBase sb)
      : SQBObject(parent.vm),
        _strategy(sb)
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
      if (t.type != OT_INSTANCE)
        throw std::runtime_error("class " + types::name<BaseClassType>() + " not registered yet");

      types::Type::create<ClassType,  ClassType*, BaseClassType, BaseClassType*>(OT_INSTANCE);
      types::Type::create<ClassType*, ClassType,  BaseClassType, BaseClassType*>(OT_INSTANCE);

      SQBClass cl(vm, parent.find(t.name));
      cl.push();
      sq_newclass(vm, true);  // [table, string, class]
    }

    // set typetag
    sq_settypetag(vm, -1, reinterpret_cast<SQUserPointer>(_strategy.thisTypetagOriginal));

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
  }

  template <typename... Args>
  SQBClass& bindConstructor() {
    //auto func = [](Args... args){ return new ClassType(args...); };
    //instanceAllocator(func);
    HSQUIRRELVM v = vm;
    SmartBase sb = _strategy;

    detail::registerFunction(this, "constructor", [v, sb](Args... args) -> void {
      auto obj = sb.pack( new ClassType(args...) );
      sq_setinstanceup(v, 1, obj );
      sq_setreleasehook(v, 1, sb.releaseHook);
      sq_pop(v, 2);
    });
    return *this;
  }

  template <typename... Args>
  SQBClass& bindConstructor(sig_t<void, Args...> s) {
    HSQUIRRELVM v = vm;
    SmartBase sb = _strategy;

    detail::registerFunction(this, "constructor", [v, sb](Args... args) -> void {
      auto obj = sb.pack( new ClassType(args...) );
      sq_setinstanceup(v, 1, obj );
      sq_setreleasehook(v, 1, sb.releaseHook);
      sq_pop(v, 2);
    }, _strategy.md(false, s.numberOptionalArguments));
    return *this;
  }

  template <typename Func>
  SQBClass& bindConstructor(const Func lambda) {
    auto func = detail::make_function(lambda);
    instanceAllocator(func);
    return *this;
  }


  // non const
  template <typename Ret, typename... Args>
  SQBClass& bindMethod(const std::string &name, Ret (ClassType::*method)(Args...))
  {
    detail::registerFunction(this, name, [method](ClassType *self, Args... args) -> Ret {
      return (self->*method)(args...);
    }, _strategy.md());
    return *this;
  }

  // const
  template <typename Ret, typename... Args>
  SQBClass& bindMethod(const std::string &name, Ret (ClassType::*method)(Args...) const)
  {
    detail::registerFunction(this, name, [method](ClassType *self, Args... args) -> Ret {
      return (self->*method)(args...);
    }, _strategy.md());
    return *this;
  }


  template <typename Func>
  SQBClass& bindMethod(const std::string &name, Func func)
  {
    detail::registerFunction(this, name, func, _strategy.md());
    return *this;
  }

  // overload
  template <typename Ret, typename... Args>
  SQBClass& bindMethod(const std::string &name, Ret (ClassType::*method)(Args...), sig_t<Ret, Args...> s)
  {
    detail::registerFunction(this, name, [method](ClassType *self, Args... args) -> Ret {
          return (self->*method)(args...);
        }, _strategy.md(false, s.numberOptionalArguments));
    return *this;
  }

  // overload const
  template <typename Ret, typename... Args>
  SQBClass& bindMethod(const std::string &name, Ret (ClassType::*method)(Args...) const, sig_t<Ret, Args...> s)
  {
    detail::registerFunction(this, name, [method](ClassType *self, Args... args) -> Ret {
          return (self->*method)(args...);
        }, _strategy.md(false, s.numberOptionalArguments));
    return *this;
  }

  template <typename Ret, typename... Args>
  SQBClass& bindStaticMethod(const std::string &name, Ret (ClassType::*method)(Args...))
  {
    detail::registerFunction(this, name, [method](Args... args) -> Ret {
      return (*method)(args...);
    }, _strategy.md(true));
    return *this;
  }

  template <typename Func>
  SQBClass& bindStaticMethod(const std::string &name, Func func) {
    detail::registerFunction(this, name, func, _strategy.md(true));
    return *this;
  }

  // overload static
  template <typename Ret, typename... Args>
  SQBClass& bindStaticMethod(const std::string &name, Ret (*method)(Args...), sig_t<Ret, Args...> s)
  {
    detail::registerFunction(this, name, method, _strategy.md(true, s.numberOptionalArguments));
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
    SmartBase sb = _strategy;

    (*_getterMap)[name] = [prop, sb](HSQUIRRELVM vm) {
      SQUserPointer up = types::popValuePointer(vm, -3);
      ClassType *c = static_cast<ClassType*>( sb.extract(up) );
      types::pushValue<PropType>(vm, c->*prop);
      return 1;
    };

    (*_setterMap)[name] = [prop, name, readOnly, sb](HSQUIRRELVM vm) {
      if (readOnly) {
        error(vm, "property '%s' read only!", name);
      }else{
        SQUserPointer up = types::popValuePointer(vm, -4);
        ClassType *c = static_cast<ClassType*>( sb.extract(up) );
        c->*prop = types::popValue<PropType>(vm, -2);
      }
      return 0;
    };

    return *this;
  }

  template<typename PropType>
  SQBClass& bindProp(const std::string &name, std::function<PropType(ClassType*)> getter, std::function<void(ClassType*, PropType)> setter = nullptr) {
    SmartBase sb = _strategy;

    if (getter) {
      (*_getterMap)[name] = [getter, sb](HSQUIRRELVM vm) {
        SQUserPointer up = types::popValuePointer(vm, -3);
        ClassType *c = static_cast<ClassType*>( sb.extract(up) );
        types::pushValue<PropType>(vm, getter(c));
        return 1;
      };
    }

    if (setter) {
      (*_setterMap)[name] = [setter, sb](HSQUIRRELVM vm) {
        SQUserPointer up = types::popValuePointer(vm, -4);
        ClassType *c = static_cast<ClassType*>( sb.extract(up) );
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
    SmartBase sb = _strategy;

    detail::registerFunction(this, "constructor", [v, sb, func](Args... args) -> void {
      auto data = sb.pack(new ClassType( func(args...) ));
      sq_setinstanceup(v, 1, data);
      sq_setreleasehook(v, 1, sb.releaseHook);
    });
  }

  template <typename Ret, typename... Args, typename std::enable_if<std::is_pointer<Ret>::value, Ret>::type* = nullptr>
  void instanceAllocator(const std::function<Ret(Args...)> func)
  {
    HSQUIRRELVM v = vm;
    SmartBase sb = _strategy;
    detail::registerFunction(this, "constructor", [v, sb, func](Args... args) -> void {
      auto obj = sb.pack( func(args...) );
      sq_setinstanceup(v, 1, obj);
      sq_setreleasehook(v, 1, sb.releaseHook);
    });
  }


  detail::FunctionMap *_setterMap;
  detail::FunctionMap *_getterMap;



  std::function<size_t(size_t(*)( ))> get_base_hash;

};

}
#endif
