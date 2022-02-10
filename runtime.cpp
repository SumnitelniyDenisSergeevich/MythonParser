#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {

    namespace {
        const string LESS_METHOD = "__lt__"s;
        const string EQ_METHOD = "__eq__"s;
        const string STR_METHOD = "__str__"s;
    }  // namespace

    ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
        : data_(std::move(data)) {
    }

    void ObjectHolder::AssertIsValid() const {
        assert(data_ != nullptr);
    }

    ObjectHolder ObjectHolder::Share(Object& object) {
        return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
    }

    ObjectHolder ObjectHolder::None() {
        return ObjectHolder();
    }

    Object& ObjectHolder::operator*() const {
        AssertIsValid();
        return *Get();
    }

    Object* ObjectHolder::operator->() const {
        AssertIsValid();
        return Get();
    }

    Object* ObjectHolder::Get() const {
        return data_.get();
    }

    ObjectHolder::operator bool() const {
        return Get() != nullptr;
    }

    bool IsTrue(const ObjectHolder& object) {
        if (object.TryAs<ValueObject<int>>()) {
            return object.TryAs<ValueObject<int>>()->GetValue() != 0;
        }
        else if (object.TryAs<ValueObject<string>>()) {
            return object.TryAs<ValueObject<string>>()->GetValue() != ""s;
        }
        else if (object.TryAs<ValueObject<bool>>()) {
            return object.TryAs<ValueObject<bool>>()->GetValue();
        }
        return false;
    }

    void ClassInstance::Print(std::ostream& os, Context& context) {
        if (this->HasMethod(STR_METHOD, 0U)) {
            this->Call(STR_METHOD, {}, context)->Print(os,context);
        }
        else {
            os << this;
        }
    }

    bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
        if (const Method* class_method = cls_.GetMethod(method); class_method) {
            if (argument_count == class_method->formal_params.size()) {
                return true;
            }
        }
        return false;
    }

    Closure& ClassInstance::Fields() {
        return closure_;
    }

    const Closure& ClassInstance::Fields() const {
        return closure_;
    }

    ClassInstance::ClassInstance(const Class& cls) : cls_(cls) {
    }

    ObjectHolder ClassInstance::Call(const std::string& method,
        const std::vector<ObjectHolder>& actual_args,
        Context& context) {

        const Method* class_method = cls_.GetMethod(method);
        if (!class_method) {
            throw std::runtime_error("undeclareted method"s);
        }
        else if (class_method->formal_params.size() != actual_args.size()) {
            throw std::runtime_error("undeclareted method"s);
        }
        Closure closure;
        closure["self"] = ObjectHolder::Share(*this);
        for (size_t i = 0; i < class_method->formal_params.size(); ++i) {
            closure[class_method->formal_params.at(i)] = actual_args.at(i);
        }
        return class_method->body->Execute(closure, context);
    }

    Class::Class(std::string name, std::vector<Method> methods, const Class* parent) : name_(move(name)), methods_(move(methods)), parent_(parent) {
    }

    const Method* Class::GetMethod(const std::string& method_name) const {
        for (const Method& method : methods_) {
            if (method.name == method_name) {
                return &method;
            }
        }
        if (parent_) {
            return parent_->GetMethod(method_name);
        }
        return nullptr;
    }

    void Class::Print(ostream& os, [[maybe_unused]] Context& context) {
        os << "Class " << name_;
    }

    void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
        os << (GetValue() ? "True"sv : "False"sv);
    }

    bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        ObjectHolder obj_holder;
        if (obj_holder = EqualObjectHolders<int>(lhs, rhs); obj_holder) {
            return obj_holder.TryAs<runtime::Bool>()->GetValue();
        }
        else if (obj_holder = EqualObjectHolders<string>(lhs, rhs); obj_holder) {
            return obj_holder.TryAs<runtime::Bool>()->GetValue();
        }
        else if (obj_holder = EqualObjectHolders<bool>(lhs, rhs); obj_holder) {
            return obj_holder.TryAs<runtime::Bool>()->GetValue();
        }
        else if (!lhs && !rhs) {
            return true;
        }
        else if (lhs.TryAs<ClassInstance>()) {
            if (lhs.TryAs<ClassInstance>()->HasMethod(EQ_METHOD, 1U)) {
                return IsTrue(lhs.TryAs<ClassInstance>()->Call(EQ_METHOD, { rhs }, context));
            }
        }
        throw std::runtime_error("diffrent tipes"s);
    }

    bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        ObjectHolder obj_holder;
        if (obj_holder = LessObjectHolders<int>(lhs, rhs); obj_holder) {
            return obj_holder.TryAs<runtime::Bool>()->GetValue();
        }
        else if (obj_holder = LessObjectHolders<string>(lhs, rhs); obj_holder) {
            return obj_holder.TryAs<runtime::Bool>()->GetValue();
        }
        else if (obj_holder = LessObjectHolders<bool>(lhs, rhs); obj_holder) {
            return obj_holder.TryAs<runtime::Bool>()->GetValue();
        }
        else if (lhs.TryAs<ClassInstance>()) {
            if (lhs.TryAs<ClassInstance>()->HasMethod(LESS_METHOD, 1U)) {
                return IsTrue(lhs.TryAs<ClassInstance>()->Call(LESS_METHOD, { rhs }, context));
            }
        }
        throw std::runtime_error("diffrent tipes"s);
    }

    bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, [[maybe_unused]] Context& context) {
        return !Equal(lhs, rhs, context);
    }

    bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, [[maybe_unused]] Context& context) {
        return (!Less(lhs, rhs, context) && !Equal(lhs, rhs, context));
    }

    bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, [[maybe_unused]] Context& context) {
        return (Less(lhs, rhs, context) || Equal(lhs, rhs, context));
    }

    bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, [[maybe_unused]] Context& context) {
        return (!Less(lhs, rhs, context));
    }

}  // namespace runtime