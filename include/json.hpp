#include <string>
#include <vector>
#include <list>
#include <cassert>
#include <cstdio>

#include <iostream>

namespace json {
  enum class JsonObjects {
    INT, FLOAT, NULL_VAL, OBJECT, ARRAY, STRING, BOOLEAN
  };
  std::string to_string(JsonObjects obj) {
    switch(obj) {
      case JsonObjects::INT: return "JsonObjects::INT";
      case JsonObjects::FLOAT: return "JsonObjects::FLOAT";
      case JsonObjects::NULL_VAL: return "JsonObjects::NULL_VAL";
      case JsonObjects::BOOLEAN: return "JsonObjects::BOOLEAN";
      case JsonObjects::OBJECT: return "JsonObjects::OBJECT";
      case JsonObjects::ARRAY: return "JsonObjects::ARRAY";
      case JsonObjects::STRING: return "JsonObjects::STRING";
    };
    return "this is bullcheating";
  }

  struct Object;
  struct Array;
  struct String;

  struct Value;

  namespace detail {
    template<typename T>
    struct Getter;
  }

  struct String {
    std::string str;
    String(std::string el) : str{std::move(el)} {}
    String(const char* el) : str{el} {}
  };

  struct Value {
    JsonObjects whatAmI;
    union {
      int i;
      float f;
      nullptr_t null;
      bool b;
      Object* object;
      Array* array;
      String* string;
    } element;

    template<typename T>
    auto get() -> decltype(detail::Getter<T>{}.get(*this)) ;

    Value(int param) : whatAmI{JsonObjects::INT} { element.i = param; }
    Value(float param) : whatAmI{JsonObjects::FLOAT} { element.f = param; }
    Value(double param) : Value(float(param)) {}
    Value(nullptr_t param) : whatAmI{JsonObjects::NULL_VAL} { element.null = param; }
    Value(bool param) : whatAmI{JsonObjects::BOOLEAN} { element.b = param; }
    Value(Object &param) : whatAmI{JsonObjects::OBJECT} { element.object = &param; }
    Value(Array &param) : whatAmI{JsonObjects::ARRAY} { element.array = &param; }
    Value(String &param) : whatAmI{JsonObjects::STRING} { element.string = &param; }
  };

  struct KeyValue {
    String key;
    Value value;
  };

  struct Object {
    std::vector<KeyValue> inner;
    void add(String key, Value val) {
      inner.push_back({std::move(key), val});
    }
  };
  struct Array {
    std::vector<Value> inner;
    void add(Value val) {
      inner.push_back(val);
    }
  };


  namespace detail {
    template<typename T>
    struct Getter;

    template<>
    struct Getter<int> {
      int get(const Value&v) {
        if (v.whatAmI == JsonObjects::INT) return v.element.i;
        std::cout << "failed: " << to_string(v.whatAmI) << std::endl;
        assert(false);
        return -1;
      }
    };
    template<>
    struct Getter<float> {
      float get(const Value&v) {
        if (v.whatAmI == JsonObjects::FLOAT) return v.element.f;
        std::cout << "failed: " << to_string(v.whatAmI) << std::endl;
        assert(false);
        return -1;
      }
    };
    template<>
    struct Getter<nullptr_t> {
      nullptr_t get(const Value&v) {
        if (v.whatAmI != JsonObjects::NULL_VAL) assert(false);
        std::cout << "failed: " << to_string(v.whatAmI) << std::endl;
        assert(false);
        return {};
      }
    };
    template<>
    struct Getter<Object> {
      const Object* get(const Value&v) {
        if (v.whatAmI == JsonObjects::OBJECT) return v.element.object;
        std::cout << "failed: " << to_string(v.whatAmI) << std::endl;
        assert(false);
        return {};
      }
      Object* get(Value&v) {
        if (v.whatAmI == JsonObjects::OBJECT) return v.element.object;
        std::cout << "failed: " << to_string(v.whatAmI) << std::endl;
        assert(false);
        return {};
      }
    };
    template<>
    struct Getter<Array> {
      const Array* get(const Value&v) {
        if (v.whatAmI == JsonObjects::ARRAY) return v.element.array;
        std::cout << "failed: " << to_string(v.whatAmI) << std::endl;
        assert(false);
        return {};
      }
      Array* get(Value&v) {
        if (v.whatAmI == JsonObjects::ARRAY) return v.element.array;
        std::cout << "failed: " << to_string(v.whatAmI) << std::endl;
        assert(false);
        return {};
      }
    };
    template<>
    struct Getter<String> {
      const String* get(const Value&v) {
        if (v.whatAmI == JsonObjects::STRING) return v.element.string;
        std::cout << "failed: " << to_string(v.whatAmI) << std::endl;
        assert(false);
        return {};
      }
      String* get(Value&v) {
        if (v.whatAmI == JsonObjects::STRING) return v.element.string;
        std::cout << "failed: " << to_string(v.whatAmI) << std::endl;
        assert(false);
        return {};
      }
    };
    template<>
    struct Getter<bool> {
      bool get(const Value&v) {
        if (v.whatAmI == JsonObjects::BOOLEAN) return v.element.b;
        std::cout << "failed: " << to_string(v.whatAmI) << std::endl;
        assert(false);
        return false;
      }
    };
  }

  template<typename T>
  auto Value::get() -> decltype(detail::Getter<T>{}.get(*this)) {
    return detail::Getter<T>{}.get(*this);
  }

  std::string to_string(bool b) {
    if(b) return "true";
    return "false";
  }
  std::string to_string(int value) {
    char buffer[20] = {};
    std::sprintf(buffer, "%d", value);
    return buffer;
  }
  std::string to_string(float value) {
    char buffer[20] = {};
    std::sprintf(buffer, "%f", value);
    return buffer;
  }
  std::string to_string(const Value &);
  std::string to_string(const KeyValue &);
  std::string to_string(const Object &);
  std::string to_string(const Array &);
  std::string to_string(const String &);

  std::string to_string(const Value &v) {
    switch(v.whatAmI) {
      case JsonObjects::INT: return to_string(v.element.i);
      case JsonObjects::FLOAT: return to_string(v.element.f);
      case JsonObjects::NULL_VAL: return "null";
      case JsonObjects::BOOLEAN: return json::to_string(v.element.b);
      case JsonObjects::OBJECT: return to_string(*v.element.object);
      case JsonObjects::ARRAY: return to_string(*v.element.array);
      case JsonObjects::STRING: return to_string(*v.element.string);
    };
    return "this is bullcheat";
  }
  std::string to_string(const KeyValue &o) {
    return to_string(o.key) + ":" + to_string(o.value);
  }
  std::string to_string(const Object &o) {
    if(o.inner.empty()) return "{}";
    std::string str = "{";
    for (int i = 0; i < o.inner.size() - 1; ++i)
      str += to_string(o.inner[i]) + ",";
    return str += to_string(o.inner.back()) + "}";
  }
  std::string to_string(const Array &a) {
    if(a.inner.empty()) return "[]";
    std::string str = "[";
    for (int i = 0; i < std::ptrdiff_t(a.inner.size()) - 1; ++i)
      str += to_string(a.inner[i]) + ",";
    return str += to_string(a.inner.back()) + "]";
  }
  std::string to_string(const String &s) {
    return "\"" + s.str + "\"";
  }

  struct JSONBuilder {
    std::list<Object> objects;
    std::list<Array> arrays;
    std::list<String> str;

    bool as_object;
    JSONBuilder(bool initially_as_object = true) : as_object{initially_as_object} {
      if(as_object) objects.emplace_back();
      else arrays.emplace_back();
    }

    struct ValueAdder {
      Value v;
      Value to(Object &o, String name) {
        o.add(name, v);
        return v;
      }
      Value to(Array &a) {
        a.add(v);
        return v;
      }
    };

    ValueAdder add(int el) { return {Value{el}}; }
    ValueAdder add(float el) { return {Value{el}}; }
    ValueAdder add(double el) { return {Value{el}}; }
    ValueAdder add(nullptr_t el) { return {Value{el}}; }
    ValueAdder add(bool el) { return {Value{el}}; }

    ValueAdder add(const Object &param) {
      
      objects.push_back(param);
      return {Value{objects.back()}};
    }
    ValueAdder add(const Array &param) {
      
      arrays.push_back(param);
      return {Value{arrays.back()}};
    }
    ValueAdder add(const String &param) {
      str.push_back(param);
      return {Value{str.back()}};
    }
    ValueAdder add(const char* el) { return add(String(el)); }

    String serialize() {
      if(as_object) return to_string(objects.front());
      else return to_string(arrays.front());
    }

    Object& baseObject() { return objects.front(); }
    Array& baseArray() { return arrays.front(); }
    Value base() { if(as_object) return {baseObject()}; else return {baseArray()}; }
  };
}
