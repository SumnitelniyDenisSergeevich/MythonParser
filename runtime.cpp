#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>

#include <iostream> //delete

using namespace std;

namespace runtime {

    ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
        : data_(std::move(data)) {
    }

    void ObjectHolder::AssertIsValid() const {
        assert(data_ != nullptr);
    }

    ObjectHolder ObjectHolder::Share(Object& object) {
        // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
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
            if (object.TryAs<ValueObject<int>>()->GetValue() == 0) {
                return false;
            }
            else {
                return true;
            }
        }
        else if (object.TryAs<ValueObject<string>>()) {
            if (object.TryAs<ValueObject<string>>()->GetValue() == ""s) {
                return false;
            }
            else {
                return true;
            }
        }
        else if (object.TryAs<ValueObject<bool>>()) {
            if (object.TryAs<ValueObject<bool>>()->GetValue() == true) {
                return true;
            }
        }
        return false;
    }

    void ClassInstance::Print(std::ostream& os, Context& context) {
        if (const Method* class_method = cls_.GetMethod("__str__"s); class_method) {
            this->Call(class_method->name, {}, context)->Print(os,context);
        }
        else {
            os << this;
        }
    }

    bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
        const Method* class_method = cls_.GetMethod(method);
        if (class_method) {
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
            throw std::runtime_error("there is no such method"s);
        }
        else if (class_method->formal_params.size() != actual_args.size()) {
            throw std::runtime_error("there is no such method"s);
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

    bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, [[maybe_unused]] Context& context) {
        if (lhs.TryAs<ValueObject<int>>() && rhs.TryAs<ValueObject<int>>()) {
            if (lhs.TryAs<ValueObject<int>>()->GetValue() == rhs.TryAs<ValueObject<int>>()->GetValue()) {
                return true;
            }
            else {
                return false;
            }
        }
        else if (lhs.TryAs<ValueObject<string>>() && rhs.TryAs<ValueObject<string>>()) {
            if (lhs.TryAs<ValueObject<string>>()->GetValue() == rhs.TryAs<ValueObject<string>>()->GetValue()) {
                return true;
            }
            else {
                return false;
            }
        }
        else if (lhs.TryAs<ValueObject<bool>>() && rhs.TryAs<ValueObject<bool>>()) {
            if (lhs.TryAs<ValueObject<bool>>()->GetValue() == rhs.TryAs<ValueObject<bool>>()->GetValue()) {
                return true;
            }
            else {
                return false;
            }
        }
        else if (lhs.Get() == nullptr && rhs.Get() == nullptr) {
            return true;
        }
        else if (lhs.TryAs<ClassInstance>()) {
            if (lhs.TryAs<ClassInstance>()->HasMethod("__eq__", 1U)) {
                return IsTrue(lhs.TryAs<ClassInstance>()->Call("__eq__"s, { rhs }, context));
            }
        }
        throw std::runtime_error("diffrent tipes"s);
    }

    bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, [[maybe_unused]] Context& context) {
        if (lhs.TryAs<ValueObject<int>>() && rhs.TryAs<ValueObject<int>>()) {
            if (lhs.TryAs<ValueObject<int>>()->GetValue() < rhs.TryAs<ValueObject<int>>()->GetValue()) {
                return true;
            }
            else {
                return false;
            }
        }
        else if (lhs.TryAs<ValueObject<string>>() && rhs.TryAs<ValueObject<string>>()) {
            if (lhs.TryAs<ValueObject<string>>()->GetValue() < rhs.TryAs<ValueObject<string>>()->GetValue()) {
                return true;
            }
            else {
                return false;
            }
        }
        else if (lhs.TryAs<ValueObject<bool>>() && rhs.TryAs<ValueObject<bool>>()) {
            if (lhs.TryAs<ValueObject<bool>>()->GetValue() < rhs.TryAs<ValueObject<bool>>()->GetValue()) {
                return true;
            }
            else {
                return false;
            }
        }
        else if (lhs.TryAs<ClassInstance>()) {
            if (lhs.TryAs<ClassInstance>()->HasMethod("__lt__", 1U)) {
                return IsTrue(lhs.TryAs<ClassInstance>()->Call("__lt__"s, { rhs }, context));
            }
        }
        throw std::runtime_error("diffrent tipes"s);
    }

    bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, [[maybe_unused]] Context& context) {
        return !Equal(lhs, rhs, context);
    }

    bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, [[maybe_unused]] Context& context) {
        if (!Less(lhs, rhs, context) && !Equal(lhs, rhs, context)) {
            return true;
        }
        return false;
    }

    bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, [[maybe_unused]] Context& context) {
        if (Less(lhs, rhs, context) || Equal(lhs, rhs, context)) {
            return true;
        }
        return false;
    }

    bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, [[maybe_unused]] Context& context) {
        if (!Less(lhs, rhs, context)) {
            return true;
        }
        return false;
    }

}  // namespace runtime