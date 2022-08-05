#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

    using runtime::Closure;
    using runtime::Context;
    using runtime::ObjectHolder;

    namespace {
        const string ADD_METHOD = "__add__"s;
        const string INIT_METHOD = "__init__"s;
    }  // namespace

    Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
        :var_(std::move(var))
        , rv_(std::move(rv)) {
    }

    ObjectHolder Assignment::Execute(Closure& closure, [[maybe_unused]] Context& context) {
        closure[var_] = rv_->Execute(closure, context);
        return closure.at(var_);
    }

    VariableValue::VariableValue(const std::string& var_name) {
        dotted_ids_.emplace_back(var_name);
    }

    VariableValue::VariableValue(std::vector<std::string> dotted_ids)
        : dotted_ids_(std::move(dotted_ids)) {
    }

    ObjectHolder VariableValue::Execute(Closure& closure, [[maybe_unused]] Context& context) {
        if (dotted_ids_.size() == 1) {
            if (closure.find(dotted_ids_[0]) == closure.end()) {
                throw std::runtime_error("VariableValue::Execute");
            }
            return closure.at(dotted_ids_[0]);
        }

        ObjectHolder object = closure.at(dotted_ids_[0]);
        runtime::ClassInstance* cl = object.TryAs<runtime::ClassInstance>();

        if (cl == nullptr) {
            throw std::runtime_error("VariableValue::Execute. runtime::ClassInstance* == nullptr"s);
        }

        for (size_t i = 1; i + 1 < dotted_ids_.size(); ++i) {
            if (cl->HasMethod(dotted_ids_[i], 0)) {
                object = cl->Fields().at(dotted_ids_[i]);
            }
            else {
                throw std::runtime_error("VariableValue::Execute. HasMethod false"s);
            }
        }

        return object.TryAs<runtime::ClassInstance>()->Fields().at(dotted_ids_.back());
    }

    unique_ptr<Print> Print::Variable(const std::string& name) {
        return std::make_unique<Print>(std::make_unique<VariableValue>(name));
    }

    Print::Print(unique_ptr<Statement> argument) {
        args_.emplace_back(std::move(argument));
    }

    Print::Print(vector<unique_ptr<Statement>> args)
        :args_(std::move(args)) {
    }

    ObjectHolder Print::Execute(Closure& closure, Context& context) {
        bool first = true;
        for (const unique_ptr<Statement>& arg : args_) {
            if (!first) {
                context.GetOutputStream() << ' ';
            }
            first = false;
            ObjectHolder object = arg->Execute(closure, context);
            if (object) {
                object->Print(context.GetOutputStream(), context);
            }
            else {
                context.GetOutputStream() << "None";
            }
        }
        context.GetOutputStream() << "\n";
        return {};
    }

    MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method, std::vector<std::unique_ptr<Statement>> args)
        : object_(std::move(object))
        , method_(method)
        , args_(std::move(args))
    {
    }

    ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
        std::vector<ObjectHolder> actual_args;
        for (const std::unique_ptr<Statement>& arg : args_) {
            actual_args.emplace_back(std::move(arg->Execute(closure, context)));
        }

        ObjectHolder obj = object_->Execute(closure, context);
        if (obj.TryAs<runtime::ClassInstance>()) {
            return obj.TryAs<runtime::ClassInstance>()->Call(method_, actual_args, context);
        }

        throw runtime_error("MethodCall::Execute");
    }

    ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
        const auto arg = GetArgument()->Execute(closure, context);
        std::ostringstream out;

        if (const auto ptr = arg.TryAs<runtime::Number>()) {
            ptr->Print(out, context);
            return ObjectHolder::Own(runtime::String(out.str()));
        }

        if (const auto ptr = arg.TryAs<runtime::String>()) {
            ptr->Print(out, context);
            return ObjectHolder::Own(runtime::String(out.str()));
        }

        if (const auto ptr = arg.TryAs<runtime::Bool>()) {
            ptr->Print(out, context);
            return ObjectHolder::Own(runtime::String(out.str()));
        }

        if (const auto ptr = arg.TryAs<runtime::ClassInstance>()) {
            ptr->Print(out, context);
            return ObjectHolder::Own(runtime::String(out.str()));
        }
        
        return ObjectHolder::Own(runtime::String("None"));
    }

    ObjectHolder Add::Execute(Closure& closure, Context& context) {
        if (!GetLhs() || !GetRhs()) {
            throw runtime_error("lhs or/and rhs are null");
        }

        ObjectHolder lhs = GetLhs()->Execute(closure, context);
        ObjectHolder rhs = GetRhs()->Execute(closure, context);

        if (lhs.TryAs<::runtime::Number>() && rhs.TryAs<::runtime::Number>()) {
            return ObjectHolder::Own(::runtime::Number(lhs.TryAs<::runtime::Number>()->GetValue() + rhs.TryAs<::runtime::Number>()->GetValue()));
        }

        if (lhs.TryAs<::runtime::String>() && rhs.TryAs<::runtime::String>()) {
            return ObjectHolder::Own(::runtime::String(lhs.TryAs<::runtime::String>()->GetValue() + rhs.TryAs<::runtime::String>()->GetValue()));
        }

        runtime::ClassInstance* cl = lhs.TryAs<runtime::ClassInstance>();

        if (cl != nullptr) {
            if (cl->HasMethod(ADD_METHOD, 1)) {
                return cl->Call(ADD_METHOD, vector{ rhs }, context);
            }
        }

        throw runtime_error("Add::Execute");
    }

    ObjectHolder Sub::Execute(Closure& closure, Context& context) {
        if (!GetLhs() || !GetRhs()) {
            throw runtime_error("lhs or/and rhs are null");
        }

        ObjectHolder lhs = GetLhs()->Execute(closure, context);
        ObjectHolder rhs = GetRhs()->Execute(closure, context);
        if (lhs.TryAs<::runtime::Number>() && rhs.TryAs<::runtime::Number>()) {
            return ObjectHolder::Own(::runtime::Number(lhs.TryAs<::runtime::Number>()->GetValue() - rhs.TryAs<::runtime::Number>()->GetValue()));
        }
        throw runtime_error("Sub::Execute");
    }

    ObjectHolder Mult::Execute(Closure& closure, Context& context) {
        if (!GetLhs() || !GetRhs()) {
            throw runtime_error("lhs or/and rhs are null");
        }

        ObjectHolder lhs = GetLhs()->Execute(closure, context);
        ObjectHolder rhs = GetRhs()->Execute(closure, context);
        if (lhs.TryAs<::runtime::Number>() && rhs.TryAs<::runtime::Number>()) {
            return ObjectHolder::Own(::runtime::Number(lhs.TryAs<::runtime::Number>()->GetValue() * rhs.TryAs<::runtime::Number>()->GetValue()));
        }
        throw runtime_error("Mult::Execute");
    }

    ObjectHolder Div::Execute(Closure& closure, Context& context) {
        if (!GetLhs() || !GetRhs()) {
            throw runtime_error("lhs or/and rhs are null");
        }

        ObjectHolder lhs = GetLhs()->Execute(closure, context);
        ObjectHolder rhs = GetRhs()->Execute(closure, context);
        if (lhs.TryAs<::runtime::Number>() && rhs.TryAs<::runtime::Number>() && rhs.TryAs<::runtime::Number>()->GetValue() != 0) {
            return ObjectHolder::Own(::runtime::Number(lhs.TryAs<::runtime::Number>()->GetValue() / rhs.TryAs<::runtime::Number>()->GetValue()));
        }
        throw runtime_error("Div::Execute");
    }

    void Compound::AddStatement(std::unique_ptr<Statement> stmt) {
        statements_.emplace_back(std::move(stmt));
    }

    ObjectHolder Compound::Execute(Closure& closure, Context& context) {
        for (const auto& statement : statements_) {
            statement->Execute(closure, context);
        }
        return {};
    }

    Return::Return(std::unique_ptr<Statement> statement) : statement_(std::move(statement)){
    }

    ObjectHolder Return::Execute(Closure& closure, Context& context) {
        throw statement_->Execute(closure, context);
    }

    ClassDefinition::ClassDefinition(ObjectHolder cls) : cls_(std::move(cls)){
    }

    ObjectHolder ClassDefinition::Execute(Closure& closure, [[maybe_unused]] Context& context) {
        closure[cls_.TryAs<runtime::Class>()->GetName()] = cls_;
        return cls_;
    }

    FieldAssignment::FieldAssignment(VariableValue object, std::string field_name, std::unique_ptr<Statement> rv)
        :object_(std::move(object))
        , field_name_(std::move(field_name))
        , rv_(std::move(rv)) {
    }

    ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
        runtime::ClassInstance* cl = object_.Execute(closure, context).TryAs<runtime::ClassInstance>();
        if (cl) {
            Closure& fields = cl->Fields();
            fields[field_name_] = rv_->Execute(closure, context);
            return fields.at(field_name_);
        }
        throw std::runtime_error("cls==nullptr");
    }

    IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body, std::unique_ptr<Statement> else_body)
        : condition_(std::move(condition))
        , if_body_(std::move(if_body))
        , else_body_(std::move(else_body))
    {
    }

    ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
        if (IsTrue(condition_->Execute(closure, context))) {
            return if_body_->Execute(closure, context);
        }
        else {
            if (else_body_) {
                return else_body_->Execute(closure, context);
            }
        }
        return {};
    }

    ObjectHolder Or::Execute(Closure& closure, Context& context) {
        ObjectHolder lhs = GetLhs()->Execute(closure, context);

        if (lhs.TryAs<::runtime::Bool>()->GetValue()) {
            return ObjectHolder::Own(::runtime::Bool(true));
        }

        ObjectHolder rhs = GetRhs()->Execute(closure, context);

        if (rhs.TryAs<::runtime::Bool>()->GetValue()) {
            return ObjectHolder::Own(::runtime::Bool(true));
        }

        return ObjectHolder::Own(::runtime::Bool(false));
    }

    ObjectHolder And::Execute(Closure& closure, Context& context) {
        ObjectHolder lhs = GetLhs()->Execute(closure, context);
        ObjectHolder rhs = GetRhs()->Execute(closure, context);

        return ObjectHolder::Own(::runtime::Bool(lhs.TryAs<::runtime::Bool>()->GetValue() && rhs.TryAs<::runtime::Bool>()->GetValue()));
    }

    ObjectHolder Not::Execute(Closure& closure, Context& context) {
        ObjectHolder arg = GetArgument()->Execute(closure, context);
        return ObjectHolder::Own(::runtime::Bool(!arg.TryAs<::runtime::Bool>()->GetValue()));
    }

    Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
        : BinaryOperation(std::move(lhs), std::move(rhs))
        , cmp_(std::move(cmp))
    {
    }

    ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
        return ObjectHolder::Own(::runtime::Bool(cmp_(GetLhs()->Execute(closure, context), GetRhs()->Execute(closure, context), context)));
    }

    NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args)
        : class__(class_)
        , args_(std::move(args))
    {
    }

    NewInstance::NewInstance(const runtime::Class& class_) : class__(class_)
    {
    }

    ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
        ObjectHolder object = ObjectHolder::Own(runtime::ClassInstance(class__));

        runtime::ClassInstance* cl = object.TryAs<runtime::ClassInstance>();
        if (cl->HasMethod(INIT_METHOD, args_.size())) {
            std::vector<ObjectHolder> actual_args;
            for (const std::unique_ptr<Statement>& arg : args_) {
                actual_args.emplace_back(std::move(arg->Execute(closure, context)));
            }
            cl->Call(INIT_METHOD, actual_args, context);
        }

        return object;
    }

    MethodBody::MethodBody(std::unique_ptr<Statement>&& body) : body_(std::move(body))
    {
    }

    ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
        ObjectHolder object = ObjectHolder::None();
        try {
            object = body_->Execute(closure, context);
        }
        catch (ObjectHolder& obj) {
            object = std::move(obj);
        }
        return object;
    }

    UnaryOperation::UnaryOperation(std::unique_ptr<Statement> argument)
        : argument_(std::move(argument))
    {
    }

    const std::unique_ptr<Statement>& UnaryOperation::GetArgument() const {
        return argument_;
    }

    BinaryOperation::BinaryOperation(std::unique_ptr<Statement> lhs, std::unique_ptr<Statement> rhs)
        : lhs_(std::move(lhs))
        , rhs_(std::move(rhs))
    {
    }

    const std::unique_ptr<Statement>& BinaryOperation::GetLhs() const {
        return lhs_;
    }

    const std::unique_ptr<Statement>& BinaryOperation::GetRhs() const {
        return rhs_;
    }
}  // namespace ast