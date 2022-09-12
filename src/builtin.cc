#include "builtin.h"
#include "format.h"

namespace autumn {
namespace builtin {

std::map<std::string, object::BuiltinFunction> BUILTINS = {
    {"len", len},
    {"first", first},
    {"last", last},
    {"push", push},
    {"rest", rest},
    {"puts", puts},
    {"range", range},
};

std::shared_ptr<object::Object> len(const std::vector<std::shared_ptr<object::Object>>& args) {
    if (args.size() != 1) {
        return std::make_shared<object::Error>(format("wrong number of arguments. expected 1, got {}", args.size()));
    }

    auto& arg = args[0];

    if (typeid(*arg) == typeid(object::String)) {
        auto obj = arg->cast<object::String>();
        return std::make_shared<object::Integer>(obj->value().length());
    } else if (typeid(*arg) == typeid(object::Array)) {
        auto obj = arg->cast<object::Array>();
        return std::make_shared<object::Integer>(obj->elements().size());
    }
    return std::make_shared<object::Error>(format("argument to `len` not supported, got {}", arg->type()));
}

std::shared_ptr<object::Object> first(const std::vector<std::shared_ptr<object::Object>>& args) {
    if (args.size() != 1) {
        return std::make_shared<object::Error>(format("wrong number of arguments. expected 1, got {}", args.size()));
    }

    auto& arg = args[0];

    if (typeid(*arg) == typeid(object::Array)) {
        auto obj = arg->cast<object::Array>();
        if (obj->elements().empty()) {
            return object::constants::Null;
        }
        return obj->elements().front();
    }
    return std::make_shared<object::Error>(format("argument to `front` not supported, got {}", arg->type()));
}

std::shared_ptr<object::Object> last(const std::vector<std::shared_ptr<object::Object>>& args) {
    if (args.size() != 1) {
        return std::make_shared<object::Error>(format("wrong number of arguments. expected 1, got {}", args.size()));
    }

    auto& arg = args[0];

    if (typeid(*arg) == typeid(object::Array)) {
        auto obj = arg->cast<object::Array>();
        if (obj->elements().empty()) {
            return object::constants::Null;
        }
        return obj->elements().back();
    }
    return std::make_shared<object::Error>(format("argument to `last` not supported, got {}", arg->type()));
}

std::shared_ptr<object::Object> push(const std::vector<std::shared_ptr<object::Object>>& args) {
    if (args.size() != 2) {
        return std::make_shared<object::Error>(format("wrong number of arguments. expected 2, got {}", args.size()));
    }

    auto& arg0 = args[0];
    auto& arg1 = args[1];

    if (typeid(*arg0) == typeid(object::Array)) {
        auto obj = arg0->cast<object::Array>();
        auto new_obj = std::make_shared<object::Array>(obj->elements());
        new_obj->append(arg1);
        return new_obj;
    }
    return std::make_shared<object::Error>(format("argument to `push` not supported, got {}", arg0->type()));
}

std::shared_ptr<object::Object> rest(const std::vector<std::shared_ptr<object::Object>>& args) {
    if (args.size() != 1) {
        return std::make_shared<object::Error>(format("wrong number of arguments. expected 1, got {}", args.size()));
    }

    auto& arg = args[0];

    if (typeid(*arg) == typeid(object::Array)) {
        auto obj = arg->cast<object::Array>();
        if (obj->elements().empty()) {
            return object::constants::Null;
        }

        auto new_obj = std::make_shared<object::Array>();
        bool first = true;
        for (auto& e : obj->elements()) {
            if (!first) {
                new_obj->append(e);
            }
            first = false;
        }
        return new_obj;
    }
    return std::make_shared<object::Error>(format("argument to `push` not supported, got {}", arg->type()));
}

std::shared_ptr<object::Object> puts(const std::vector<std::shared_ptr<object::Object>>& args) {
    for (auto& e : args) {
        std::cout << e->inspect() << std::endl;
    }
    return object::constants::Null;
}

std::shared_ptr<object::Object> range(const std::vector<std::shared_ptr<object::Object>>& args) {
    if (args.size() != 1 && args.size() != 2) {
        return std::make_shared<object::Error>(format("wrong number of arguments. expected 1 or 2, got {}", args.size()));
    }

    for (auto &arg : args) {
        if (typeid(*arg) != typeid(object::Integer)) {
            return std::make_shared<object::Error>(
                format("argument type to `range` is int, get {}", arg->type())
            );
        }
    }
    
    int l = 0, r = 0;
    r = args.back()->cast<object::Integer>()->value();
    if (args.size() == 2) {
        l = args.front()->cast<object::Integer>()->value();
    }

    auto obj = std::make_shared<object::Array>();
    for (int i = l; i < r; ++i) {
        obj->append(std::make_shared<object::Integer>(i));
    }
    return obj;
}

} // namespace builtin
} // namespace autumn
