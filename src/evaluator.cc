#include "evaluator.h"
#include "builtin.h"

#include <thread>

namespace autumn {

Evaluator::Evaluator() :
    _env(new object::Environment()) {
}
 
std::shared_ptr<const object::Object> Evaluator::eval(const std::string& input) {
    auto program = _parser.parse(input);
    return eval(program.get(), _env);
}

bool Evaluator::is_error(const object::Object* obj) const {
    return typeid(*obj) == typeid(object::Error);
}

void Evaluator::reset_env() {
    _env.reset(new object::Environment());
}

std::shared_ptr<object::Object> Evaluator::eval(
        const ast::Node* node,
        std::shared_ptr<object::Environment>& env) const {
    

    if (node == nullptr) {
        std::string message;
        for (size_t i = 0; i < _parser.errors().size(); ++i) {
            auto& error = _parser.errors()[i];
            if (i != 0) {
                message.append(1, '\n');
            }
            message.append(error);
        }
        return new_error("abort: {}", message);
    }

    if (typeid(*node) == typeid(ast::Program)) {
        auto n = node->cast<ast::Program>();
        return eval_program(n->statments(), env);

    } else if (typeid(*node) == typeid(ast::ExpressionStatment)) {
        auto n = node->cast<ast::ExpressionStatment>();
        return eval(n->expression(), env);

    } else if (typeid(*node) == typeid(ast::BlockStatment)) {
        auto n = node->cast<ast::BlockStatment>();
        return eval_statments(n->statments(), env);

    } else if (typeid(*node) == typeid(ast::ReturnStatment)) {
        auto n = node->cast<ast::ReturnStatment>();
        auto return_val = eval(n->expression(), env);
        if (is_error(return_val.get())) {
            return return_val;
        }
        return std::make_shared<object::ReturnValue>(return_val);

    } else if (typeid(*node) == typeid(ast::LetStatment)) {
        auto n = node->cast<ast::LetStatment>();
        auto val = eval(n->expression(), env);
        if (is_error(val.get())) {
            return val;
        }

        env->set(n->identifier()->value(), val);

    } else if (typeid(*node) == typeid(ast::IntegerLiteral)) {
        auto n = node->cast<ast::IntegerLiteral>();
        return std::shared_ptr<object::Integer>(
                new object::Integer(n->value()));

    } else if (typeid(*node) == typeid(ast::BooleanLiteral)) {
        auto n = node->cast<ast::BooleanLiteral>();
        return n->value() ? object::constants::True : object::constants::False;

    } else if (typeid(*node) == typeid(ast::StringLiteral)) {
        auto n = node->cast<ast::StringLiteral>();
        return std::make_shared<object::String>(n->value());

    } else if (typeid(*node) == typeid(ast::ArrayLiteral)) {
        auto n = node->cast<ast::ArrayLiteral>();
        auto elems = eval_expressions(n->elements(), env);
        if (!elems.empty() && is_error(elems[0].get())) {
            return elems[0];
        }
        return std::make_shared<object::Array>(elems);

    } else if (typeid(*node) == typeid(ast::HashLiteral)) {
        auto n = node->cast<ast::HashLiteral>();
        return eval_hash_literal(n, env);

    } else if (typeid(*node) == typeid(ast::PrefixExpression)) {
        auto n = node->cast<ast::PrefixExpression>();
        auto right = eval(n->right(), env);
        if (is_error(right.get())) {
            return right;
        }
        return eval_prefix_expression(n->op(), right.get(), env);

    } else if (typeid(*node) == typeid(ast::InfixExpression)) {
        auto n = node->cast<ast::InfixExpression>();
        auto left = eval(n->left(), env);
        if (is_error(left.get())) {
            return left;
        }

        auto right = eval(n->right(), env);
        if (is_error(right.get())) {
            return right;
        }

        return eval_infix_expression(n->op(), left.get(), right.get(), env);

    } else if (typeid(*node) == typeid(ast::IfExpression)) {
        return eval_if_expression(node->cast<ast::IfExpression>(), env);

    } else if (typeid(*node) == typeid(ast::Identifier)) {
        return eval_identifier(node->cast<ast::Identifier>(), env);

    } else if (typeid(*node) == typeid(ast::FunctionLiteral)) {
        auto n = node->cast<ast::FunctionLiteral>();
        return std::make_shared<object::Function>(
                n->parameters(), n->body(), env);

    } else if (typeid(*node) == typeid(ast::CallExpression)) {
        auto n = node->cast<ast::CallExpression>();
        auto function = eval(n->function(), env);
        if (is_error(function.get())) {
            return function;
        }

        auto args = eval_expressions(n->arguments(), env);
        if (!args.empty() && is_error(args[0].get())) {
            return args[0];
        }

        std::shared_ptr<object::Object>* true_obj_ptr = new std::shared_ptr<object::Object>;

        std::thread t([this, function, args=std::move(args)](std::shared_ptr<object::Object>* obj) mutable {
            *obj = apply_function(function.get(), args);
        }, true_obj_ptr);

        return std::make_shared<object::Async>(std::move(t), true_obj_ptr);

        // return apply_function(function.get(), args);

    } else if (typeid(*node) == typeid(ast::IndexExpression)) {
        auto n = node->cast<ast::IndexExpression>();
        auto array = eval(n->left(), env);
        if (is_error(array.get())) {
            return array;
        }

        auto index = eval(n->index(), env);
        if (is_error(index.get())) {
            return index;
        }

        return eval_index_expression(array.get(), index.get());

    }

    return nullptr;
}

std::shared_ptr<object::Object> Evaluator::eval_index_expression(
        const object::Object* obj,
        const object::Object* index) const {
    if (typeid(*obj) == typeid(object::Async)) {
        obj = obj->cast<object::Async>()->object().get();
    }
    if (typeid(*index) == typeid(object::Async)) {
        index = index->cast<object::Async>()->object().get();
    }
    if (typeid(*obj) == typeid(object::Array)
            && typeid(*index) == typeid(object::Integer)) {
        auto a = obj->cast<object::Array>();
        auto i = index->cast<object::Integer>();

        auto& elems = a->elements();
        auto idx = i->value();

        if (idx < 0) {
            idx += elems.size();
        }

        if (idx < 0 || idx >= elems.size()) {
            return object::constants::Null;
        }

        return elems[idx];
    } else if (typeid(*obj) == typeid(object::Hash)) {
        auto h = obj->cast<object::Hash>();
        return h->get(index);
    }

    return new_error("index operator not supported: {}`{}`{}",
            color::light::light,
            obj->type(),
            color::off);
}

std::shared_ptr<object::Object> Evaluator::apply_function(
        const object::Object* fn,
        std::vector<std::shared_ptr<object::Object>>& args) const {

    std::shared_ptr<object::Object> val = object::constants::Null;

    if (typeid(*fn) == typeid(object::Async)) {
        fn = fn->cast<object::Async>()->object().get();
    }
    for (auto &object : args) {
        if (typeid(*object) == typeid(object::Async)) {
            object = object->cast<object::Async>()->object();
        }
    }

    if (typeid(*fn) == typeid(object::Function)) {
        auto function = fn->cast<object::Function>();
        auto extended_env = extend_function_env(function, args);
        // 开始执行函数体内的语句
        val = eval(function->body(), extended_env);
    } else if (typeid(*fn) == typeid(object::Builtin)) {
        auto builtin_fn = fn->cast<object::Builtin>();
        val = builtin_fn->run(args);
    }

    if (val == nullptr) {
        return nullptr;
    }

    if (typeid(*val) == typeid(object::ReturnValue)) {
        return val->cast<object::ReturnValue>()->value();
    }
    return val;
}

std::shared_ptr<object::Environment> Evaluator::extend_function_env(
        const object::Function* fn,
        std::vector<std::shared_ptr<object::Object>>& args) const {
    auto new_env = std::make_shared<object::Environment>(fn->env());
    auto& params = fn->parameters();

    for (size_t i = 0; i < params.size() && i < args.size(); ++i) {
        new_env->set(params[i]->value(), args[i]);
    }
    return new_env;
}

std::vector<std::shared_ptr<object::Object>> Evaluator::eval_expressions(
        const std::vector<std::unique_ptr<ast::Expression>>& exps,
        std::shared_ptr<object::Environment>& env) const {
    std::vector<std::shared_ptr<object::Object>> results;
    for (auto& exp : exps) {
        auto val = eval(exp.get(), env);
        if (is_error(val.get())) {
            return { val };
        }
        results.emplace_back(val);
    }
    return results;
}

std::shared_ptr<object::Object> Evaluator::eval_program(
        const std::vector<std::unique_ptr<ast::Statment>>& statments,
        std::shared_ptr<object::Environment>& env) const {
    std::shared_ptr<object::Object> result;

    for (int i = 0; i < (int)statments.size(); ++i) {
        auto& stat = statments[i];
        result = eval(stat.get(), env);
        if (result == nullptr) {
            continue;
        }

        if (typeid(*result) == typeid(object::Async)) {
            if (i == (int)statments.size() - 1) {
                result = result->cast<object::Async>()->object();
            }
            else {
                result->cast<object::Async>()->detach();
            }
        }

        if (typeid(*result) == typeid(object::ReturnValue)) {
            return result->cast<object::ReturnValue>()->value();
        } else if (typeid(*result) == typeid(object::Error)) {
            return result;
        }
    }
    return result;
}

std::shared_ptr<object::Object> Evaluator::eval_identifier(
        const ast::Identifier* identifier,
        std::shared_ptr<object::Environment>& env) const {
    auto val = env->get(identifier->value());
    if (val != nullptr) {
        return val;
    }

    auto builtin_fn = builtin::BUILTINS.find(identifier->value());
    if (builtin_fn != builtin::BUILTINS.end()) {
        return std::make_shared<object::Builtin>(builtin_fn->second);
    }

    return new_error("identifier not found: {}`{}`{}",
            color::light::light,
            identifier->value(),
            color::off);
}

std::shared_ptr<object::Object> Evaluator::eval_statments(
        const std::vector<std::unique_ptr<ast::Statment>>& statments,
        std::shared_ptr<object::Environment>& env) const {
    std::shared_ptr<object::Object> result;

    for (auto& stat : statments) {
        result = eval(stat.get(), env);
        if (result == nullptr) {
            continue;
        }

        if (typeid(*result) == typeid(object::ReturnValue)
                || typeid(*result) == typeid(object::Error)) {
            return result;
        }
    }
    return result;
}

std::shared_ptr<object::Object> Evaluator::eval_prefix_expression(
        const std::string& op,
        const object::Object* right,
        std::shared_ptr<object::Environment>& env) const {

    if (typeid(*right) == typeid(object::Async)) {
        right = right->cast<object::Async>()->object().get();
    }

    if (op == "!") {
        return eval_bang_operator_expression(right);
    } else if (op == "-") {
        return eval_minus_prefix_operator_expression(right);
    }

    return new_error("unknown operator: {}`{}{}`{}",
            color::light::light,
            op,
            right->type(),
            color::off);
}

std::shared_ptr<object::Object> Evaluator::eval_bang_operator_expression(const object::Object* right) const {
    if (right == object::constants::Null.get()) {
        return object::constants::True;
    } else if (right == object::constants::True.get()) {
        return object::constants::False;
    } else if (right == object::constants::False.get()) {
        return object::constants::True;
    }
    return object::constants::False;
}

std::shared_ptr<object::Object> Evaluator::eval_minus_prefix_operator_expression(const object::Object* right) const {
    if (typeid(*right) != typeid(object::Integer)) {
        return new_error("unknown operator: {}`-{}`{}",
                color::light::light,
                right->type(),
                color::off);
    }

    auto result = right->cast<object::Integer>();
    return std::make_shared<object::Integer>(-result->value());
}


std::shared_ptr<object::Object> Evaluator::native_bool_to_boolean_object(bool input) const {
    return input ? object::constants::True : object::constants::False;
};

std::shared_ptr<object::Object> Evaluator::eval_integer_infix_expression(
        const std::string& op,
        const object::Object* left,
        const object::Object* right,
        std::shared_ptr<object::Environment>& env) const {
    auto left_val = left->cast<object::Integer>();
    auto right_val = right->cast<object::Integer>();

    if (op == "+") {
        return std::make_shared<object::Integer>(left_val->value() + right_val->value());
    } else if (op == "-") {
        return std::make_shared<object::Integer>(left_val->value() - right_val->value());
    } else if (op == "*") {
        return std::make_shared<object::Integer>(left_val->value() * right_val->value());
    } else if (op == "/") {
        return std::make_shared<object::Integer>(left_val->value() / right_val->value());
    } else if (op == "<") {
        return native_bool_to_boolean_object(left_val->value() < right_val->value());
    } else if (op == "<=") {
        return native_bool_to_boolean_object(left_val->value() <= right_val->value());
    } else if (op == ">") {
        return native_bool_to_boolean_object(left_val->value() > right_val->value());
    } else if (op == ">=") {
        return native_bool_to_boolean_object(left_val->value() >= right_val->value());
    } else if (op == "==") {
        return native_bool_to_boolean_object(left_val->value() == right_val->value());
    } else if (op == "!=") {
        return native_bool_to_boolean_object(left_val->value() != right_val->value());
    }

    return new_error("unknown operator: {}`{} {} {}`{}",
            color::light::light,
            left->type(), op, right->type(),
            color::off);
}

std::shared_ptr<object::Object> Evaluator::eval_string_infix_expression(
        const std::string& op,
        const object::Object* left,
        const object::Object* right,
        std::shared_ptr<object::Environment>& env) const {
    auto left_val = left->cast<object::String>();
    auto right_val = right->cast<object::String>();

    if (op == "+") {
        return std::make_shared<object::String>(left_val->value() + right_val->value());
    }

    return new_error("unknown operator: {}`{} {} {}`{}",
            color::light::light,
            left->type(), op, right->type(),
            color::off);
}

std::shared_ptr<object::Object> Evaluator::eval_array_infix_expression(
        const std::string& op,
        const object::Object* left,
        const object::Object* right,
        std::shared_ptr<object::Environment>& env) const {
    auto left_val = left->cast<object::Array>();
    auto right_val = right->cast<object::Array>();

    if (op == "+") {
        auto ret = std::make_shared<object::Array>(left_val->elements());
        for (auto& e : right_val->elements()) {
            ret->append(e);
        }
        return ret;
    }

    return new_error("unknown operator: {}`{} {} {}`{}",
            color::light::light,
            left->type(), op, right->type(),
            color::off);
}

std::shared_ptr<object::Object> Evaluator::eval_infix_expression(
        const std::string& op,
        const object::Object* left,
        const object::Object* right,
        std::shared_ptr<object::Environment>& env) const {

    if (typeid(*left) == typeid(object::Async)) {
        left = left->cast<object::Async>()->object().get();
    }
    if (typeid(*right) == typeid(object::Async)) {
        right = right->cast<object::Async>()->object().get();
    }    

    // 如果你想对其它类型做单独处理，改这个位置即可
    // 原作者在其书中调侃：十年后，当 Monkey 语言出名后，可能会有人在 stackoverflow 上提问：
    // 为什么在 Monkey 语言中(当前我们的项目叫 Autum)，整型值的比较比其它类型要慢呢？
    // 此时你可以回复：balabala...，来自：M78 星云，Allen
    if (typeid(*left) == typeid(object::Integer)
            && typeid(*right) == typeid(object::Integer)) {
        return eval_integer_infix_expression(op, left, right, env);
    } else if (typeid(*left) == typeid(object::String)
            && typeid(*right) == typeid(object::String)) {
        return eval_string_infix_expression(op, left, right, env);
    } else if (typeid(*left) == typeid(object::Array)
            && typeid(*right) == typeid(object::Array)) {
        return eval_array_infix_expression(op, left, right, env);
    } else if (typeid(*left) != typeid(*right)) {
        return new_error("type mismatch: {}`{} {} {}`{}",
                color::light::light,
                left->type(), op, right->type(),
                color::off);
    } else if (op == "==") {
        // 非整数类型，直接比较对象指针
        // 如果是 Boolean 类型，因为全局都是用的同一份，所以指针相同
        return native_bool_to_boolean_object(left == right);
    } else if (op == "!=") {
        return native_bool_to_boolean_object(left != right);
    }
    return new_error("unknown operator: {}`{} {} {}`{}",
            color::light::light,
            left->type(), op, right->type(),
            color::off);
}

bool Evaluator::is_truthy(const object::Object* obj) const {
    if (obj == object::constants::Null.get()) {
        return false;
    } else if (obj == object::constants::True.get()) {
        return true;
    } else if (obj == object::constants::False.get()) {
        return false;
    }
    return true;
}

std::shared_ptr<object::Object> Evaluator::eval_if_expression(
            const ast::IfExpression* exp,
            std::shared_ptr<object::Environment>& env) const {
    if (exp->condition() == nullptr) {
        return object::constants::Null;
    }
    auto condition = eval(exp->condition(), env);
    if (is_error(condition.get())) {
        return condition;
    }

    if (is_truthy(condition.get()) && exp->consequence() != nullptr) {
        return eval(exp->consequence(), env);
    } else if (exp->alternative() != nullptr) {
        return eval(exp->alternative(), env);
    }

    return object::constants::Null;
}

std::shared_ptr<object::Object> Evaluator::eval_hash_literal(
            const ast::HashLiteral* exp,
            std::shared_ptr<object::Environment>& env) const {
    auto ret = std::make_shared<object::Hash>();

    auto& pairs = exp->pairs();
    // std::pair<std::unique_ptr<ast::Expression>, std::unique_ptr<ast::Expression>>
    for (auto& pair : pairs) {
        auto key = eval(pair.first.get(), env);

        if (key == nullptr) {
            return nullptr;
        }

        auto val = eval(pair.second.get(), env);

        if (val == nullptr) {
            return nullptr;
        }

        ret->append(key, val);
    }

    return ret;
}

} // namespace autumn
