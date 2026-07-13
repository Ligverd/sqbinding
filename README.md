![C++11](https://img.shields.io/badge/C%2B%2B-11-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-green)

# SQBinding

Binding to the scripting language [Squirrel](http://www.squirrel-lang.org/).

Pure C++11, no macros, no external dependencies — only STL and Squirrel.

This code was written as part of another major project. I thought some parts might be useful to other people.
I wanted to unify the C++ API and the API for writing scripts as much as possible. To make the same code in C++ and Squirrel look almost the same.

## Why Squirrel

At first, I used Lua. Objectively speaking, it's a great thing. But the syntax was completely unsuitable for my task. The existing bindings also did not solve all my needs.  Squirrel, with its classes, tables, and syntax, is more similar to C++.

The problem was that, once again, ready-made bindings did not provide what was needed.

**What SQBinding can do**

- Ultra-lightweight — less than 3000 lines of clean C++ code
- Multiple independent VMs: `sqb::SQBinding vm1, vm2, vm3;`
- Can wrap an already running VM: `sqb::SQBinding sqb(raw_vm);`
- Bind free functions, lambdas, and methods
- Overloaded functions, methods, and static methods via `sqb::sig<Ret, Args...>()`
- Optional arguments via `sqb::sig<Ret, Args...>(NumberOptionalArguments)` or `{NumberOptionalArguments}`
- Fluent interface for class binding: `.bindConstructor().bindMethod().bindProp()`
- Native any smart pointer support via `sqb::Smart<T>()`
- Inheritance — base class methods available in derived classes, `instanceof` works
- Bind C++ operators to Squirrel metamethods (`_shiftl`, `_or`)
- Bidirectional value binding — bind C++ variables to script and modify from both sides
- Tables and arrays with STL-compatible access
- Call Squirrel functions from C++ and pass/return any bound type
- Custom types via `popValue`/`pushValue` specialization
- Execute scripts from strings or files

## Tested Environments

The project has been successfully built and tested on the following configurations:


| Operating System | Compiler | Status |
| :--- | :--- | :---: |
| **macOS** | Apple Clang 16.0 | [PASS] |
| **Linux** | GCC 11.2 | [PASS] |
| **Linux** | Clang 13.0 | [PASS] |
| **Windows** | MSVC 19.51 (Visual Studio 2026) | [PASS] |


## Automatic Dependency Management ("Batteries Included")

SQBinding completely automates the lifecycle of the Squirrel scripting language. You don't need to download, build, or link Squirrel manually.

* **Embedded & Patched Squirrel:** By default, SQBinding automatically downloads the correct version of Squirrel from GitHub, applies the `01_meta_method.patch` (enabling advanced metamethods like `_shiftl` and `_or`), and builds it as a static library along with your project.
* **Zero Configuration:** Integration is as simple as adding `add_subdirectory` in CMake. All internal dependencies, compiler flags, and linkings are handled automatically.
* **System Squirrel Support:** For package maintainers or specific environments, you can optionally force the use of an unpatched system-wide Squirrel library by toggling a CMake cache variable:

```cmake
set(USE_INTERNAL_SQUIRREL OFF CACHE BOOL "Use system squirrel" FORCE)
```

## What it looks like in a real project

The goal was to get a tool with which you can easily get the same API in C++ and Squirrel.

**Squirrel:**
```lua
od   <- ObjectDetection()
http <- HttpServer("0.0.0.0", 9876)

c <- Chain()
c << Demuxer(url, {rtsp_transport="tcp"}) << VIDEO << Decoder() 
  << Scale({width=640,format="bgr24"}) 
  << MotionDetector(od) 
  << Reemit({fps=1}) << Encoder("libx264", h264_par) 
  << Muxer({format="mpegts"}) << http.createResource("md")

c.start()
```

**C++:**
```cpp
ObjectDetection od;
HttpServer http("0.0.0.0", 9876);

Chain c;
c << make::Demuxer(url, {{"rtsp_transport","tcp"}}) << MTM::VIDEO << make::Decoder() 
  << make::Scale({{"width",640},{"format","bgr24"}}) 
  << make::MotionDetector(od) 
  << make::Reemit({{"fps",1}}) << make::Encoder("libx264", h264_par) 
  << make::Muxer({{"format","mpegts"}}) << http.createResource("md");

c.start();
```

The difference is minimal. This is exactly what was intended.


**Real example of binding**

```cpp
sqb.bindClass<Chain>("Chain")
    .bindConstructor()
    .bindMethod("start", &Chain::start)
    .bindMethod("stop", &Chain::stop)
    .bindMethod("clean", &Chain::clean)
    .bindMethod("reset", &Chain::reset)
    .bindMethod("empty", &Chain::empty)
    .bindMethod("getInformation", &Chain::getInformation)
    .bindMethod("getSegmentsInformation", &Chain::getSegmentsInformation)
    .bindMethod("getSvg", &Chain::getSvg, {1})
    .bindMethod("_shiftl", &Chain::operator<<, sqb::sig<Chain&,const char*>())
    .bindMethod("_shiftl", &Chain::operator<<, sqb::sig<Chain&,MediaTypeMask>())
    .bindMethod("_shiftl", &Chain::operator<<, sqb::sig<Chain&,Connector::Number>())
    .bindMethod("_shiftl", &Chain::operator<<, sqb::sig<Chain&,std::shared_ptr<Component>>())
    .bindMethod("_shiftl", &Chain::operator<<, sqb::sig<Chain&,Chain::Tag>())
    .bindProp<std::string>("name", [](Chain *self) { return self->getName(); }, [](Chain *self, std::string name){ self->setName(name); })
;


sqb.bindClass<HttpServer>("HttpServer")
    .bindConstructor<std::string,int>()
    .bindConstructor<std::string,int,Dictionary>()
    .bindConstructor<std::string>()
    .bindConstructor<std::string,Dictionary>()
    .bindMethod("start", &HttpServer::start)
    .bindMethod("stop",  &HttpServer::stop)
    .bindMethod("createResource", &HttpServer::createResource, {1})
    .bindMethod("removeResource", &HttpServer::removeResource)
    .bindMethod("addMountPoint" , &HttpServer::addMountPoint)
    .bindMethod("removeMountPoint", &HttpServer::removeMountPoint)
    .bindMethod("addHandler", [&sqb](HttpServer *self, std::string uriPattern, sqb::SQBFunction func, std::string mime) {
        Server::Handler handler(uriPattern, [&sqb, func, mime](Server::Request &req, Server::Response &res){
            std::string content = func(req);
            if (content.empty()) {
                res.set_content("not found", 404, "text/plain");
            }else{
                res.set_content(content.c_str(), 200, mime.c_str());
            }
        }, "");
        self->addHandler(handler);
    })
    .bindMethod("addRoute", &HttpServer::addRoute)
;

```

# Features

## Minimal example

Call the Squirrel function from C++ and vice versa the C++ function from Squirrel.

```cpp
#include "sqbinding.h"

void hellocpp() {
  printf("hello C++\n");
}

int main(int argc, char **argv)
{
  sqb::SQBinding sqb;

  sqb.bindFunction("hellocpp", hellocpp);
  sqb.executeString(R"(
    hellocpp()

    function hellosquirrel() {
      print("hello Squirrel\n")
    }
  )");

  auto hellosquirrel = sqb.getFunction("hellosquirrel");

  hellosquirrel();

  return 0;
}
```

Console output is disabled by default. You must provide your own output function — this way it integrates directly into your logging system.

```cpp
void printfunc(HSQUIRRELVM v, const SQChar *s,...)
{
  va_list vl;
  va_start(vl, s);
  vfprintf(stdout, s, vl);
  fprintf(stdout, "\n");
  fflush(stdout);
  va_end(vl);
}

sqb::SQBinding sqb;
sq_setprintfunc(sqb.vm, printfunc, printfunc); // print func, error func
```

Wrap your code in a try-catch block — SQBinding throws exceptions on errors. Using a separate {...} scope is recommended.

```cpp

int main() {
  ...

  int result = 0;

  {
    sqb::SQBinding sqb;
    sq_setprintfunc(sqb.vm, printfunc, printfunc);

    try {
      result = sqb.executeFile("script.nut");
    }catch(const std::exception& e){
      fprintf(stderr, "Error %s : %s", e.what(), sqb::detail::getCurrentPosition(sq.sqb.vm).c_str());
      std::exit(EXIT_FAILURE);
    }

    result = Event::Loop();
  }

  ...

  return result;
}

```

Important: all bound variables, types, and classes exist until sqb is destroyed. This lets you load base functions and classes from one file, then execute another file that uses them, or run small scripts from strings.


## Binding of functions

Three ways: a pointer to a function, std::function, or lambda

```cpp

// 1. pointer to a function
std::string func(std::string name) {
  return "Hello " + name;
}

sqb.bindFunction("func", func);

// 2. std::function
std::function<std::string(std::string)> func;

func = [](std::string name) {
  return "Hello " + name;
};

sqb.bindFunction("func", func);

// 3. lambda
sqb.bindFunction("func", [](std::string name){
  return "Hello " + name;
});

```

Now you can call **func() from Squirrel.**

The real trouble is overloaded functions. SQBinding solves this cleanly.

```cpp
int test(int i) { return i; }
std::string test(std::string s) { return s; }

sqb.bindFunction("test", test, sqb::sig<int,int>());
sqb.bindFunction("test", test, sqb::sig<std::string,std::string>());
```

No static_cast, no macros. Just sqb::sig<Ret, Args...>() to resolve the overload.

If there is only one function or method, but it contains optional arguments, just specify their number.


```cpp
int test(int i, int a = 0) { return i + a; }

sqb.bindFunction("test", test, sqb::sig<int,int,int>(1));
```

or compact version if not use overload

```cpp
int test(int i, int a = 0) { return i + a; }

sqb.bindFunction("test", test, {1});
```



The reverse — calling a Squirrel function from C++.

```lua
function func(name) {
  return "Hello " + name
}
```

First you need to execute the Squirrel code, then try to call the Squirrel function.

```cpp

sqb.executeFile("script.nut");
// or
sqb.executeString(script);

// creating the SQBFunction object
auto func = sqb.getFunction("func");

// call, ignore return value
func("Ligverd");

// implicit conversion to return type
std::string str = func("Ligverd");

// explicit return type
std::string str = func("Ligverd").ret<std::string>(); 

```

Script passes the callback to C++

```lua
camera.onEvent(function(event) {
  print("Detected: " + event.type);
});
```

C++ accepts and saves

```cpp
sqb.bindClass<Camera>("Camera")
   .bindMethod("onEvent", [](Camera* self, sqb::SQBFunction callback) {
      self->setCallback([callback](EventData e) {
        callback(e);
      });
      return self;
   });
```

## Working with variables

Three operations: bind, set, and get.

```cpp
std::string name;

sqb.bindValue("name", &name);
sqb.bindValue("name", &name, true); // read only

// get the value of a variable from Squirrel, if there is no variable, there will be an exception
auto value = sqb.getValue<std::string>("value");

// get if available
std::string value;
sqb.getValueIfExists("value", value);

// setting the value
std::string value = "val";
sqb.setValue("value", value);

```

## Custom types

Basic scalar types are built in. To pass your own type, specialize popValue and pushValue.

```cpp

struct CustomType
{
  std::string name;
  int number;
  std::vector<int> ar;
};


namespace sqb {
namespace types {

template<>
inline CustomType popValue<CustomType>(HSQUIRRELVM vm, SQInteger idx) {
  if (sq_gettype(vm, idx) != OT_TABLE)
    throw std::runtime_error("can't convert to CustomType");

  CustomType val;

  SQBTable t = popValue<SQBTable>(vm, idx);

  val.name   = t.getValue<std::string>("name");
  val.number = t.getValue<int>("number");
  val.ar     = t.getArray("ar").to_vector<int>();

  return val;
}

template<>
inline void pushValue<CustomType>(HSQUIRRELVM vm, const CustomType& val) {
  SQBTable t(vm);
  SQBArray ar(vm);

  ar.append(val.ar);

  t.setValue("name", val.name);
  t.setValue("number", val.number);
  t.setValue("ar", ar);

  t.push();
}

}}
```

If you bind a class, you don't need these converters — SQBinding handles type compatibility automatically.

For edge cases, you can manually declare type compatibility:


```cpp
types::Type::create<ClassType,  ClassType*, BaseClassType, BaseClassType*>(OT_INSTANCE);
```

This is rarely needed — for example, when a base class is not bound, but a child class is, and a function expects the base type.


## Working with tables as a separate type and as a namespace

Squirrel has no structs, hashes, or namespaces — everything is a table. Global functions, variables, and types live in the root table. sqb::SQBinding sqb is itself an SQBTable wrapping the root table.

```cpp
sqb::SQBinding sqb; // <- this is the SQBTable on the Squirrel root_table

// create a table in the root table
auto my = sqb.newTable("my");

// bind and set variables and functions on a new table
std::string name;
int index;
int func(int i) {...}

sqb.newTable("my")
  .bindValue("name", &name)
  .bindValue("index", &index)
  .bindFunction("func", func)
  .bindFunction("lambda", [](int x) { return x*2; })
  .newTable("subtable")
    .bindValue("subname", &name)
  ;

```

In Squirrel it looks like this


```lua
print(my.name)
my.index = 5;

my.func(8)
my.lambda(9)

my.subtable.name = "hello"
```

To get a table from Squirrel into C++:

```cpp
sqb::SQBTable t(sqb.vm, sqb.find("my"));
```

`find` works on any `SQBTable`, returning the native `HSQOBJECT`.

## Arrays

Arrays are represented by `SQBArray`. You rarely need to use it directly — mainly in pushValue/popValue converters or manual unpacking.

```cpp
// add a field in the table with the Array type
auto ar = sqb.newArray("name");
ar.append(1)
  .append(2)
  .append("Hello")
  .append(object)
  ;

// create an array in your table
auto ar = sqb.newTable("my").newArray("name");
```

You can also create a standalone array — it won't be placed on the Squirrel stack.

```cpp
sqb::SQBArray ar(sqb.vm);

// add a data collection immediately
ar.append(std::vector<int>({1,73,3,4,5}));

// adding one element at a time
ar.append(42);

// accessing the index
ar[1] = "hello";

// conversion to type automatically
std::string s = ar[1];

// explicit type conversion
std::string s = ar[1].as<std::string>();

// number of elements
ar.size();

// clear the array
ar.clear();

```

In Squirrel, elements of the same array can have mixed types. But a single type is more often used. To do this, you can use the conversion.


```cpp
sqb::SQBArray ar(sqb.vm);
ar.append(std::vector<int>({1,73,3,4,5}));

auto v = ar.to_vector<int>();
```

As I did in C++11, there is no `std::variant` in it, but you can use C++17 and higher or write your own type analog `variant`

in this case, you will get more convenience.

```cpp
sqb::SQBArray ar(sqb.vm);

using Type = std::variant<int, std::string>;

std::vector<Type> v = {1, 2, "hello"};

ar.append(v);

auto v2 = ar.to_vector<Type>();

```

But as I said, you will need conversion more often. For a struct, it is more convenient to do this via popValue/pushValue.


## Classes

In some ways, working with classes is somewhat like working with tables.

Binding is supported:
- Class
- Inheritance
- Method (via pointer, via lambda)
- Static method (via pointer, via lambda)
- Property (via pointer, via setter/getter)
- Overloading constructors and methods
- Containers (shared_ptr, unique_ptr/etc/custom)

What is not supported by SQBinding due to Squirrel limitations:
- Static properties

It is common practice to bind two static setter/getter methods, in Squirrel it may look like this.

```lua
local obj = Object()

obj.setStaticName(name)
name = obj.getStaticName()
```
Yeah, I don't like it either. If I get my hands on it, maybe I'll make a patch for Squirrel, but I do not know how much work it will take for this.

I have not allocated anything separate for the `enum`, and I usually do this


```cpp
enum Type {
  UNKNOWN,
  MOUSE,
  KEYBOARD,
  PRINTER
};

// define in the roottable
sqb.setValue("UNKNOWN",  Type::UNKNOWN);
sqb.setValue("MOUSE",    Type::MOUSE);
sqb.setValue("KEYBOARD", Type::KEYBOARD);
sqb.setValue("PRINTER",  Type::PRINTER);

// or wrap it in a table
sqb.newTable("Type")
   .setValue("UNKNOWN",  Type::UNKNOWN)
   .setValue("MOUSE",    Type::MOUSE)
   .setValue("KEYBOARD", Type::KEYBOARD)
   .setValue("PRINTER",  Type::PRINTER)
   ;

```

### Class binding


```cpp
class Base
{
public:
  int  id;
  Type type;

  std::string name;

  Base(Type type = UNKNOWN) : id(-1), type(type) {}
  Base(int id, Type type = UNKNOWN) : id(id), type(type) {}

  int getID() { return id; }

  std::string method() { return "ok"; }
  std::string method(int i) { return std::to_string(i); }
  std::string method(std::string n) { return n; }

  Base& operator+(int i) { id+=i; return *this; }

  static void resetAll() {}
  static int version;
};


sqb.bindClass<Base>("Base")
   .bindConstructor()
   .bindConstructor<Type>()
   .bindConstructor<int>()
   .bindConstructor<int, Type>()
   .bindConstructor([](std::string x){ return new Base( std::stoi(x) ); }) // <-- non-existent custom constructor
   .bindMethod("getID", &Base::getID)
   .bindMethod("method", &Base::method, sqb::sig<std::string>())
   .bindMethod("method", &Base::method, sqb::sig<std::string, int>())
   .bindMethod("method", &Base::method, sqb::sig<std::string, std::string>())
   .bindMethod("incID", [](Base *self){ self->id++; }) // <-- a custom method that does not exist
   .bindStaticMethod("resetAll", &Base::resetAll)
   .bindProp("id", &Base::id)
   .bindProp("type", &Base::type, true) // <-- true - readonly
   .bindProp<int>("next", [](Base *self){ return self->id+1; }) // <-- getter only
   .bindProp<std::string>("name", [](Base *self){ return self->name; }, [](Base *self, std::string n){ self->name = n; })
   .bindStaticMethod("getStaticVersion", [](){ return Base::version })
   .bindStaticMethod("setStaticVersion", [](int v){ Base::version = v; })
   .bindMethod("_add", &Base::operator+)
   ;

```

As I wrote above about the static property limitation, but such cases are rare.

`_add` is one of the metamethods of Squirrel itself.

**Basic metamethods**

```
_add
_sub
_mul
_div
_unm
_modulo
_set
_get
_typeof
_nexti
_cmp
_call
_cloned
_newslot
_delslot
_tostring
_newmember
_inherited
```

**Those that are available after the patch** 01_meta_method.patch


```
_shiftl
_shiftr
_and
_or
```

### Inheritance

Everything is the same here as in any OOP, as I wrote above, type compatibility will be respected automatically.

```cpp
class Base {...};
class Device : public Base {...};

sqb.bindClass<Base>("Base");
sqb.bindClass<Device, Base>("Device");
```

Squirrel stores a pointer to a class, like a regular raw pointer, which is not always convenient. For example, we create a class in Squirrel that takes over C++ and vice versa.

In these cases, it is convenient to use std::shared_ptr / std::unique_ptr / or even some kind of custom ptr.

Don't worry — you don't have to do anything special. Just bind in the same way, specifying in the constructor that you want to use the Smart Container.

In 90% of cases, the usual Smart class is enough for you, it supports all types of smart pointers with an interface like shared_ptr.
That is, if you create your CustomPtr and implement a constructor through which you can pass a raw pointer and the extractor .get() method, you can also use the Smart class.


```cpp
Smart<std::shared_ptr> sc;
Smart<std::unique_ptr> sc;
Smart<MyPtr> sc; // look there demo.cpp
```

In practice, it looks like this:


```cpp
sqb.bindClass<Base>("Base", sqb::Smart<std::shared_ptr>())
  ... then it's exactly the same

```

SQBinding will create std::shared_ptr<Base> and give it to Squirrel.

You can also fully implement your own Smart Class, but it is much easier to adapt the existing container to the shared_ptr interface.

### Important: passing this and an instance of the class.

When a method is bound using a lambda expression, it always gets an raw pointer to the type of this class as the first argument, regardless of whether the class is registered as a regular instance or via Smart.

It works the same for a regular class and for a class via Smart !!!


```cpp
sqb.bindClass<Base>("Base")
   .bindMethod("transform", [](Base* self, int i) { self->id = i; });

// or

sqb.bindClass<Base>("Base", sqb::Smart<std::shared_ptr>())
   .bindMethod("transform", [](Base* self, int i) { self->id = i; });

```

But when passing an object to an external function, the type of pointer being passed depends on how the class is registered:


```lua
local p <- Base("test")
externalFunction(p) -- passed to Base* (raw pointer)

local psp <- BaseSharedPtr("test")
externalFunction(psp) -- passed to std::shared_ptr<Base>

local pcc <- BaseCustomContainer("test")
externalFunction(pcc) -- passes the custom container specified in smart() MyPtr<Base> etc
```

Accordingly, the C++ function must be declared for the desired type:


```cpp
// For a regular instance
void externalFunction(Base* p);

// For shared_ptr
void externalFunction(std::shared_ptr<Base> p);

// For a custom container
void externalFunction(MyPtr<Base> p);
```

## Quick start

```bash
# Prepare
git clone https://github.com/Ligverd/sqbinding.git
cd sqbinding
mkdir .build && cd .build

# Build (universal)
cmake .. -DBUILD_EXAMPLES=ON
cmake --build . --config Release

# (Linux/macOS)
./examples/Release/test
./examples/Release/demo

# Windows
.\examples\Release\test.exe
.\examples\Release\demo.exe


# Other build methods

# Linux/macOS (make)
cmake .. -DBUILD_EXAMPLES=ON
make

# Ninja
cmake .. -GNinja -DBUILD_EXAMPLES=ON
ninja

# Windows (Visual Studio):
cmake .. -G "Visual Studio 17 2022" -DBUILD_EXAMPLES=ON
msbuild sqb.sln /p:Configuration=Release
```

## Add SQBinding in your project

If used CMake


```cmake
# Example: add sqb in your project
cmake_minimum_required(VERSION 3.14)

project(my_app VERSION 1.0.0 LANGUAGES C CXX)

# Use sqb from source (recommended)
add_subdirectory(external/sqb)

# Use installed sqb
# find_package(sqb REQUIRED)

# Optional: Pass options to sqb (if using add_subdirectory)
#set(USE_INTERNAL_SQUIRREL ON CACHE BOOL "Use internal squirrel" FORCE)  # ON  default
#set(BUILD_EXAMPLES OFF CACHE BOOL "Don't build sqb examples" FORCE)     # OFF default

# Your application
add_executable(my_app src/main.cpp)

# Link with sqb (all dependencies including squirrel are automatic)
target_link_libraries(my_app PRIVATE sqb)

# If you need additional include directories
# target_include_directories(my_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
```
