#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {

    namespace {
        const string EQ_METHOD = "__eq__"s;
        const string LT_METHOD = "__lt__"s;
        const string STR_METHOD = "__str__"s;

        template <typename Predicate>
        bool Compare(const ObjectHolder& lhs, const ObjectHolder& rhs, const string method, Context& context, Predicate pred) {

            if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>()) {
                return pred(lhs.TryAs<Bool>()->GetValue(),  rhs.TryAs<Bool>()->GetValue());
            }

            if (lhs.TryAs<Number>() && rhs.TryAs<Number>()) {
                return pred(lhs.TryAs<Number>()->GetValue(), rhs.TryAs<Number>()->GetValue());
            }

            if (lhs.TryAs<String>() && rhs.TryAs<String>()) {
                return pred(lhs.TryAs<String>()->GetValue(), rhs.TryAs<String>()->GetValue());
            }

            if (lhs.TryAs<ClassInstance>() && rhs.TryAs<ClassInstance>()) {
                if (lhs.TryAs<ClassInstance>()->HasMethod(method, 1)) {
                    return IsTrue(lhs.TryAs<ClassInstance>()->Call(method, { rhs }, context));
                }
                else {
                    return false;
                }
            }

            throw std::runtime_error("Can not compare objects"s);
        }


    }  // namespace

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
        return !(!object.Get() ||
            object.TryAs<Class>() ||
            object.TryAs<ClassInstance>() ||
            (object.TryAs<Bool>() && !object.TryAs<Bool>()->GetValue()) ||
            (object.TryAs<Number>() && !object.TryAs<Number>()->GetValue()) ||
            (object.TryAs<String>() && object.TryAs<String>()->GetValue().empty()));
    }
    
    void ClassInstance::Print(std::ostream& os, [[maybe_unused]] Context& context) {
        if (HasMethod(STR_METHOD, 0)) {
            Call(STR_METHOD, {}, context)->Print(os, context);
        }
        else {
            os << this;
        }
    }

    bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
        const Method* q_method = cls_.GetMethod(method);
        return q_method && q_method->formal_params.size() == argument_count;
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

        const Method* q_method = cls_.GetMethod(method);

        if (q_method && HasMethod(method, actual_args.size())) {
            Closure closure;
            closure["self"] = ObjectHolder::Share(*this);

            for (size_t i = 0; i < actual_args.size(); ++i) {
                closure[q_method->formal_params[i]] = actual_args[i];
            }

            return q_method->body->Execute(closure, context);
        }
        else {
            throw std::runtime_error("Not implemented"s);
        }
    }

    Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
        : name_(std::move(name))
        , methods_(std::move(methods))
        , parent_(parent) {
    }

    const Method* Class::GetMethod(const std::string& name) const {
        for (const Method& method : methods_) {
            if (method.name == name) {
                return &method;
            }
        }

        if (parent_ != nullptr) {
            return parent_->GetMethod(name);
        }

        return nullptr;
    }

    [[nodiscard]] const std::string& Class::GetName() const {
        return name_;
    }

    void Class::Print(ostream& os, [[maybe_unused]] Context& context) {
        os << "Class "sv << name_;
    }

    void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
        os << (GetValue() ? "True"sv : "False"sv);
    }

    bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        if (lhs.Get() == nullptr && rhs.Get() == nullptr) {
            return true;
        }

        return Compare(lhs, rhs, EQ_METHOD, context, std::equal_to());
    }

    bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return Compare(lhs, rhs, LT_METHOD, context, std::less());
    }
    
    bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, [[maybe_unused]] Context& context) {
        return !Equal(lhs, rhs, context);
    }

    bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, [[maybe_unused]] Context& context) {
        return !(Less(lhs, rhs, context) || Equal(lhs, rhs, context));
    }

    bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, [[maybe_unused]] Context& context) {
        return Less(lhs, rhs, context) || Equal(lhs, rhs, context);
    }

    bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, [[maybe_unused]] Context& context) {
        return !Less(lhs, rhs, context);
    }
}  // namespace runtime