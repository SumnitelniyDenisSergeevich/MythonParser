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
        else if (std::holds_alternative<vector<string>>(value_)) {          //"подумать"
            
            ObjectHolder ob_h;
            vector<string>& values = get<vector<string>>(value_);
            if (values.size() == 1) {
                return VariableValue{ *values.begin() }.Execute(closure, context);
            }
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
        if (argument_) {
            if (argument_->Execute(closure, context).TryAs<runtime::String>()) {
                closure.at(argument_->Execute(closure, context).TryAs<runtime::String>()->GetValue()).Get()->Print(context.GetOutputStream(), context);
                context.GetOutputStream() << '\n';
            }
        }
        else if (args_.size()) {
            bool b = false;
            for (const unique_ptr<Statement>& arg : args_) {
                if (b) {
                    context.GetOutputStream() << ' ';
                }
                b = true;
                ObjectHolder obj_h = arg->Execute(closure, context);
                if (!obj_h.Get()) {
                    context.GetOutputStream() << "None";
                    continue;
                }
                obj_h->Print(context.GetOutputStream(), context);
            }
            context.GetOutputStream() << '\n';
        }
        else {
            context.GetOutputStream() << '\n';
        }
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
        if (!argument_->Execute(closure, context).Get()) {
            return ObjectHolder::Own(runtime::String("None"s));
        }
        ostringstream o_stream;
        argument_->Execute(closure, context).Get()->Print(o_stream, context);
        return ObjectHolder::Own(runtime::String(o_stream.str()));
    }

    ObjectHolder Add::Execute(Closure& closure, Context& context) {
        if (lhs_->Execute(closure, context).TryAs<runtime::ValueObject<int>>() && rhs_->Execute(closure, context).TryAs<runtime::ValueObject<int>>()) {
            int result = lhs_->Execute(closure, context).TryAs<runtime::ValueObject<int>>()->GetValue() + rhs_->Execute(closure, context).TryAs<runtime::ValueObject<int>>()->GetValue();
            return runtime::ObjectHolder::Own(runtime::Number(result));
        }
        else if (lhs_->Execute(closure, context).TryAs<runtime::ValueObject<string>>() && rhs_->Execute(closure, context).TryAs<runtime::ValueObject<string>>()) {
            string result = lhs_->Execute(closure, context).TryAs<runtime::ValueObject<string>>()->GetValue() + rhs_->Execute(closure, context).TryAs<runtime::ValueObject<string>>()->GetValue();
            return runtime::ObjectHolder::Own(runtime::String(result));
        }
        else if (lhs_->Execute(closure, context).TryAs<runtime::ClassInstance>()) {
            if (lhs_->Execute(closure, context).TryAs<runtime::ClassInstance>()->HasMethod("__add__"s, 1U)) {
                return lhs_->Execute(closure, context).TryAs<runtime::ClassInstance>()->Call("__add__"s, { rhs_->Execute(closure, context) }, context);
            }
        }
        throw std::runtime_error("diffrent tipes or nullptr"s);
    }

    ObjectHolder Sub::Execute(Closure& closure, Context& context) {
        if (lhs_->Execute(closure, context).TryAs<runtime::ValueObject<int>>() && rhs_->Execute(closure, context).TryAs<runtime::ValueObject<int>>()) {
            int result = lhs_->Execute(closure, context).TryAs<runtime::ValueObject<int>>()->GetValue() - rhs_->Execute(closure, context).TryAs<runtime::ValueObject<int>>()->GetValue();
            return runtime::ObjectHolder::Own(runtime::Number(result));
        }
        throw std::runtime_error("diffrent tipes or nullptr"s);
    }

    ObjectHolder Mult::Execute(Closure& closure, Context& context) {
        if (lhs_->Execute(closure, context).TryAs<runtime::ValueObject<int>>() && rhs_->Execute(closure, context).TryAs<runtime::ValueObject<int>>()) {
            int result = lhs_->Execute(closure, context).TryAs<runtime::ValueObject<int>>()->GetValue() * rhs_->Execute(closure, context).TryAs<runtime::ValueObject<int>>()->GetValue();
            return runtime::ObjectHolder::Own(runtime::Number(result));
        }
        throw std::runtime_error("diffrent tipes or nullptr"s);
    }

    ObjectHolder Div::Execute(Closure& closure, Context& context) {
        if (lhs_->Execute(closure, context).TryAs<runtime::ValueObject<int>>() && rhs_->Execute(closure, context).TryAs<runtime::ValueObject<int>>()) {
            if (rhs_->Execute(closure, context).TryAs<runtime::ValueObject<int>>()->GetValue() == 0) {
                throw std::runtime_error("diffrent tipes or nullptr"s);
            }
            int result = lhs_->Execute(closure, context).TryAs<runtime::ValueObject<int>>()->GetValue() / rhs_->Execute(closure, context).TryAs<runtime::ValueObject<int>>()->GetValue();
            return runtime::ObjectHolder::Own(runtime::Number(result));
        }
        throw std::runtime_error("diffrent tipes or nullptr"s);
    }

    ObjectHolder Compound::Execute(Closure& closure, Context& context) {
        for (unique_ptr<Statement>& stmt : stmts_) {
            if (Return* d = dynamic_cast<Return*>(stmt.get()); d != nullptr) {
                return d->Execute(closure, context);
            }
            if (IfElse* d = dynamic_cast<IfElse*>(stmt.get()); d != nullptr) {
                if (ObjectHolder obj_h = d->Execute(closure, context); obj_h.Get()) {
                    return obj_h;
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
        else if (else_body_.get()) {
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
        if (const runtime::Method* class_method = cls_.GetMethod("__init__"s); class_method) {
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