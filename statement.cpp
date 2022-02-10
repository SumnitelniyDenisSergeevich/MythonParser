#include "statement.h"

#include <iostream>
#include <sstream>

#include <type_traits>

using namespace std;

namespace ast {

    using runtime::Closure;
    using runtime::Context;
    using runtime::ObjectHolder;

    namespace {
        const string ADD_METHOD = "__add__"s;
        const string INIT_METHOD = "__init__"s;
    }  // namespace

    ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
        return closure[var_] = rv_->Execute(closure, context);
    }

    Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv) : var_(var), rv_(move(rv)) {
    }

    VariableValue::VariableValue(const std::string& var_name) : value_(var_name) {
    }

    VariableValue::VariableValue(std::vector<std::string> dotted_ids) : value_(move(dotted_ids)) {
    }

    ObjectHolder VariableValue::Execute(Closure& closure, Context& context) {
        if (std::holds_alternative<const string>(value_)) {
            const string value = std::get<const string>(value_);
            if (closure.find(value) != closure.end()) {
                return closure.at(value);
            }
        }
        else if (std::holds_alternative<vector<string>>(value_)) {
            vector<string>& values = get<vector<string>>(value_);
            if (values.size() == 1) {
                return VariableValue{ *values.begin() }.Execute(closure, context);
            }
            ObjectHolder ob_h;
            for (size_t i = 0; i + 1 < values.size(); ++i) {
                if (closure.find(values.at(i)) != closure.end()) {
                    const auto& fields = closure.at(values.at(i)).TryAs<runtime::ClassInstance>()->Fields();
                    if (fields.find(values.at(i + 1)) != fields.end()) {
                        ob_h = fields.at(values.at(i + 1));
                    }
                    else {
                        throw std::runtime_error("undefined value");
                    }
                }
                else {
                    throw std::runtime_error("undefined value");
                }
            }
            return ob_h;
        }
        throw std::runtime_error("undefined value");
    }

    unique_ptr<Print> Print::Variable(const std::string& name) {
        return make_unique<Print>(Print{ make_unique<StringConst>(name) });
    }

    Print::Print(unique_ptr<Statement> argument) : argument_(move(argument)) {
    }

    Print::Print(vector<unique_ptr<Statement>> args) : args_(move(args)) {
    }

    ObjectHolder Print::Execute(Closure& closure, Context& context) {
        std::ostream& output = context.GetOutputStream();
        if (argument_) {
            if (argument_->Execute(closure, context).TryAs<runtime::String>()) {
                closure.at(argument_->Execute(closure, context).TryAs<runtime::String>()->GetValue())->Print(output, context);
            }
        }
        else if (args_.size()) {
            bool space_flag = false;
            for (const unique_ptr<Statement>& arg : args_) {
                if (space_flag) {
                    output << ' ';
                }
                space_flag = true;
                ObjectHolder obj_holder = arg->Execute(closure, context);
                if (!obj_holder) {
                    output << "None";
                    continue;
                }
                obj_holder->Print(output, context);
            }
        }
        output << '\n';
        return {};
    }

    MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
        std::vector<std::unique_ptr<Statement>> args) : object_(move(object)), method_(move(method)), args_(move(args)) {
    }

    ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
        vector<runtime::ObjectHolder> args;
        for (const auto& arg : args_) {
            args.push_back(arg->Execute(closure, context));
        }
        return object_->Execute(closure, context).TryAs<runtime::ClassInstance>()->Call(method_, args, context);
    }

    ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
        ObjectHolder obj_holder = argument_->Execute(closure, context);
        if (!obj_holder) {
            return ObjectHolder::Own(runtime::String("None"s));
        }
        ostringstream o_stream;
        obj_holder->Print(o_stream, context);
        return ObjectHolder::Own(runtime::String(o_stream.str()));
    }

    ObjectHolder Add::Execute(Closure& closure, Context& context) {
        ObjectHolder lhs_obj_h = lhs_->Execute(closure, context);
        ObjectHolder rhs_obj_h = rhs_->Execute(closure, context);
        ObjectHolder obj_holder;
        if (obj_holder = AddStatements<int,runtime::Number>(lhs_obj_h, rhs_obj_h); obj_holder) {
            return obj_holder;
        }else if (obj_holder = AddStatements<string, runtime::String>(lhs_obj_h, rhs_obj_h); obj_holder) {
            return obj_holder;
        }
        else if (lhs_obj_h.TryAs<runtime::ClassInstance>()) {
            if (lhs_obj_h.TryAs<runtime::ClassInstance>()->HasMethod(ADD_METHOD, 1U)) {
                return lhs_obj_h.TryAs<runtime::ClassInstance>()->Call(ADD_METHOD, { rhs_obj_h }, context);
            }
        }
        throw std::runtime_error("diffrent tipes or nullptr"s);
    }

    ObjectHolder Sub::Execute(Closure& closure, Context& context) {
        ObjectHolder lhs_obj_h = lhs_->Execute(closure, context);
        ObjectHolder rhs_obj_h = rhs_->Execute(closure, context);
        if (lhs_obj_h.TryAs<runtime::ValueObject<int>>() && rhs_obj_h.TryAs<runtime::ValueObject<int>>()) {
            int lhs = lhs_obj_h.TryAs<runtime::ValueObject<int>>()->GetValue();
            int rhs = rhs_obj_h.TryAs<runtime::ValueObject<int>>()->GetValue();
            int result = lhs - rhs;
            return runtime::ObjectHolder::Own(runtime::Number(result));
        }
        throw std::runtime_error("diffrent tipes or nullptr"s);
    }

    ObjectHolder Mult::Execute(Closure& closure, Context& context) {
        ObjectHolder lhs_obj_h = lhs_->Execute(closure, context);
        ObjectHolder rhs_obj_h = rhs_->Execute(closure, context);
        if (lhs_obj_h.TryAs<runtime::ValueObject<int>>() && rhs_obj_h.TryAs<runtime::ValueObject<int>>()) {
            int lhs = lhs_obj_h.TryAs<runtime::ValueObject<int>>()->GetValue();
            int rhs = rhs_obj_h.TryAs<runtime::ValueObject<int>>()->GetValue();
            int result = lhs * rhs;
            return runtime::ObjectHolder::Own(runtime::Number(result));
        }
        throw std::runtime_error("diffrent tipes or nullptr"s);
    }

    ObjectHolder Div::Execute(Closure& closure, Context& context) {
        ObjectHolder lhs_obj_h = lhs_->Execute(closure, context);
        ObjectHolder rhs_obj_h = rhs_->Execute(closure, context);
        if (lhs_obj_h.TryAs<runtime::ValueObject<int>>() && rhs_obj_h.TryAs<runtime::ValueObject<int>>()) {
            if (rhs_obj_h.TryAs<runtime::ValueObject<int>>()->GetValue() == 0) {
                throw std::runtime_error("diffrent tipes or nullptr"s);
            }
            int lhs = lhs_obj_h.TryAs<runtime::ValueObject<int>>()->GetValue();
            int rhs = rhs_obj_h.TryAs<runtime::ValueObject<int>>()->GetValue();
            int result = lhs / rhs;
            return runtime::ObjectHolder::Own(runtime::Number(result));
        }
        throw std::runtime_error("diffrent tipes or nullptr"s);
    }

    ObjectHolder Compound::Execute(Closure& closure, Context& context) {
        for (unique_ptr<Statement>& stmt : stmts_) {
            if (Return* return_ptr = dynamic_cast<Return*>(stmt.get()); return_ptr != nullptr) {
                return return_ptr->Execute(closure, context);
            }
            if (IfElse* if_else_ptr = dynamic_cast<IfElse*>(stmt.get()); if_else_ptr != nullptr) {
                if (ObjectHolder obj_holder = if_else_ptr->Execute(closure, context); obj_holder) {
                    return obj_holder;
                }
            }
            else {
                stmt->Execute(closure, context);
            }
        }
        return {};
    }

    ObjectHolder Return::Execute(Closure& closure, Context& context) {
        return statement_->Execute(closure, context);
    }

    ClassDefinition::ClassDefinition(ObjectHolder cls) : cls_(cls) {
    }

    ObjectHolder ClassDefinition::Execute(Closure& closure, [[maybe_unused]] Context& context) {
        closure[cls_.TryAs<runtime::Class>()->GetName()] = cls_;
        return  cls_;
    }

    FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
        std::unique_ptr<Statement> rv) : object_(object), field_name_(field_name), rv_(move(rv)) {

    }

    ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
        return  object_.Execute(closure, context).TryAs<runtime::ClassInstance>()->Fields()[field_name_] = rv_->Execute(closure, context);
    }

    IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
        std::unique_ptr<Statement> else_body) : condition_(move(condition)), if_body_(move(if_body)), else_body_(move(else_body)) {
    }

    ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
        if (runtime::IsTrue(condition_->Execute(closure, context))) {
            return if_body_->Execute(closure, context); 
        }
        else if (else_body_) {
            return else_body_->Execute(closure, context);
        }
        return {};
    }

    ObjectHolder Or::Execute(Closure& closure, Context& context) {
        if (runtime::IsTrue(lhs_->Execute(closure, context))) {
            return ObjectHolder::Own(runtime::Bool{ true });
        }
        else if (runtime::IsTrue(rhs_->Execute(closure, context))) {
            return ObjectHolder::Own(runtime::Bool{ true });
        }
        return ObjectHolder::Own(runtime::Bool{ false });
    }

    ObjectHolder And::Execute(Closure& closure, Context& context) {
        if (runtime::IsTrue(lhs_->Execute(closure, context)) ) {
            if (runtime::IsTrue(rhs_->Execute(closure, context))) {
                return ObjectHolder::Own(runtime::Bool{ true });
            }
        }
        return ObjectHolder::Own(runtime::Bool{ false });
    }

    ObjectHolder Not::Execute(Closure& closure, Context& context) {
        return ObjectHolder::Own(runtime::Bool{ !runtime::IsTrue(argument_->Execute(closure, context)) });
    }

    Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
        : BinaryOperation(std::move(lhs), std::move(rhs)), cmp_(cmp) {
    }

    ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
        return ObjectHolder::Own(runtime::Bool{ cmp_(lhs_->Execute(closure, context), rhs_->Execute(closure, context), context) });
    }

    NewInstance::NewInstance(const runtime::Class& cls, std::vector<std::unique_ptr<Statement>> args) : cls_(cls), args_(move(args)) {
    }

    NewInstance::NewInstance(const runtime::Class& cls) : cls_(cls) {
    }

    ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
        ObjectHolder ob_h = ObjectHolder::Own(runtime::ClassInstance{ cls_ });
        if (const runtime::Method* class_method = cls_.GetMethod(INIT_METHOD); class_method) {
            vector<ObjectHolder> args;
            for (const auto& arg : args_) {
                args.push_back(arg->Execute(closure, context));
            }
            ob_h.TryAs<runtime::ClassInstance>()->Call(class_method->name, args, context);
        }
        return ob_h;
    }

    MethodBody::MethodBody(std::unique_ptr<Statement>&& body) : body_(move(body)) {
    }

    ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
        return body_->Execute(closure, context);
    }
}  // namespace ast