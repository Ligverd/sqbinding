#include <iostream>
#include <cstdarg>
#include <cassert>
#include <vector>
#include <memory>
#include "sqbinding.h"
#include "ut/check.h"



void printfunc(HSQUIRRELVM v, const SQChar *s,...)
{
  va_list vl;
  va_start(vl, s);
  vfprintf(stdout, s, vl);
  fprintf(stdout, "\n");
  fflush(stdout);
  va_end(vl);
}

/////////////////////////////////////////////////////////////

class Base {
protected:
  int id;
  std::string name;

  static int _sec;
public:
  std::string description;

  Base()
    : id( next_id() )
  {}

  Base(const std::string &name)
    : id( next_id() ),
      name(name)
  {}

  Base(int id)
    : id(id)
  {}

  ~Base() {
    printf("Base destroy\n");
  }

  void echo() {
    std::cout << name << " id:" << id << std::endl;
  }

  Base& set_id(int i) {
    id = i;
    return *this;
  }

  Base& set_name(std::string s) {
    name = s;
    return *this;
  }

  int static next_id() {
    return ++_sec;
  }
};

int Base::_sec = -1;

class Cat : Base {
public:
  Cat()
    : Base("Cat")
  {}

  ~Cat() {
    printf("Cat destroy\n");
  }

  std::string jump() {
    return "Cat jump";
  }
};

class Bird : Base {
public:
  Bird()
    : Base("Bird")
  {}

  ~Bird() {
    printf("Bird destroy\n");
  }

  std::string fly() {
    return "Bird fly";
  }
};

class Collect {
  std::vector<std::shared_ptr<Base>> animals;

public:
  Collect() {}
  ~Collect() {
    printf("Collect destroy\n");
  }

  void push(std::shared_ptr<Base> a) {
    animals.push_back(a);
  }

  void show() {
    for (const auto &a : animals) {
      a->echo();
    }
  }
};

// your container class, just for an example. (std::shared_ptr interface)
template<typename T>
class MyPtr
{
  T* pointer;

  MyPtr(T *p) : pointer(p) {}
  T* get() { return pointer; }
};


void init(sqb::SQBinding &sqb) {
  sqb.bindClass<Base>("Base", sqb::Smart<std::shared_ptr>())
      .bindConstructor()
      .bindConstructor<std::string>()
      .bindConstructor<int>()
      .bindMethod("echo", &Base::echo)
      .bindMethod("set_id", &Base::set_id)
      .bindMethod("set_name", &Base::set_name)
      .bindStaticMethod("next_id", &Base::next_id)
      .bindProp("description", &Base::description)
      ;

  sqb.bindClass<Cat, Base>("Cat", sqb::Smart<std::shared_ptr>())
      .bindConstructor()
      .bindMethod("jump", &Cat::jump)
      // lambda variant .bindMethod("jump", [](Cat *self){ return self->jump(); })
      ;

  sqb.bindClass<Bird, Base>("Bird", sqb::Smart<std::shared_ptr>())
      .bindConstructor()
      .bindMethod("fly", &Bird::fly)
      // lambda variant .bindMethod("fly", [](Bird *self){ return self->fly(); })
      ;

  sqb.bindClass<Collect>("Collect")
      .bindConstructor()
      .bindMethod("push", &Collect::push)
      .bindMethod("show", &Collect::show)
      ;

}

int main(int argc, char **argv)
{
  int result = 0;

  {
    sqb::SQBinding sqb;
    sq_setprintfunc(sqb.vm, printfunc, printfunc);

    init(sqb);

    try {
      result = sqb.executeFile(ut::Check::getPath(__FILE__)+"/demo.nut");
    }catch(const std::exception& e){
      fprintf(stderr, "Error %s : %s", e.what(), sqb::detail::getCurrentPosition(sqb.vm).c_str());
      std::exit(500);
    }

  }

  return result;
}



