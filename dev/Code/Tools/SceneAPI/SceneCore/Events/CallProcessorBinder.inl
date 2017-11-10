/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <AzCore/std/typetraits/is_base_of.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Events
        {
            template<typename Class, typename ContextType>
            void CallProcessorBinder::BindToCall(ProcessingResult(Class::*Func)(ContextType& context) const, TypeMatch typeMatch)
            {
                AZ_STATIC_ASSERT((AZStd::is_base_of<CallProcessorBinder, Class>::value), 
                    "CallProcessorBinder can only bind to classes derived from it.");
                AZ_STATIC_ASSERT((AZStd::is_base_of<ICallContext, ContextType>::value), 
                    "Only arguments derived from ICallContext are accepted by CallProcessorBinder");

                if (typeMatch == TypeMatch::Exact)
                {
                    using Binder = ConstFunctionBindingTemplate<Class, ContextType>;
                    m_bindings.emplace_back(new Binder(Func));
                }
                else
                {
                    using Binder = ConstDerivedFunctionBindingTemplate<Class, ContextType>;
                    m_bindings.emplace_back(new Binder(Func));
                }
            }

            template<typename Class, typename ContextType>
            void CallProcessorBinder::BindToCall(ProcessingResult(Class::*Func)(ContextType& context), TypeMatch typeMatch)
            {
                AZ_STATIC_ASSERT((AZStd::is_base_of<CallProcessorBinder, Class>::value),
                    "CallProcessorBinder can only bind to classes derived from it.");
                AZ_STATIC_ASSERT((AZStd::is_base_of<ICallContext, ContextType>::value),
                    "Only arguments derived from ICallContext are accepted by CallProcessorBinder");

                if (typeMatch == TypeMatch::Exact)
                {
                    using Binder = FunctionBindingTemplate<Class, ContextType>;
                    m_bindings.emplace_back(new Binder(Func));
                }
                else
                {
                    using Binder = DerivedFunctionBindingTemplate<Class, ContextType>;
                    m_bindings.emplace_back(new Binder(Func));
                }
            }

            // FunctionBinding
            template<typename Class, typename ContextType, typename Function>
            ProcessingResult CallProcessorBinder::FunctionBinding::Call(CallProcessorBinder* thisPtr, ICallContext* context, Function function)
            {
                ContextType* arg = azrtti_cast<ContextType*>(context);
                if (arg)
                {
                    // As the compiler can't "see" the target Class for conversion the safety checks in azrtti_cast
                    //      throw a false positive. Instead of using azrtti_cast directly, so address look up here
                    //      and use a standard reinterpret_cast.
                    void* address = thisPtr->RTTI_AddressOf(Class::TYPEINFO_Uuid());
                    AZ_Assert(address, "Unable to case CallProcessorBinder to %s.", Class::TYPEINFO_Name());
                    return (reinterpret_cast<Class*>(address)->*(function))(*arg);
                }
                else
                {
                    AZ_Assert(arg, "CallProcessorBinder failed to cast context for unknown reasons.");
                    return ProcessingResult::Failure;
                }
            }

            // ConstFunctionBindingTemplate
            template<typename Class, typename ContextType>
            CallProcessorBinder::ConstFunctionBindingTemplate<Class, ContextType>::ConstFunctionBindingTemplate(Function function)
                : m_function(function)
            {
            }

            template<typename Class, typename ContextType>
            ProcessingResult CallProcessorBinder::ConstFunctionBindingTemplate<Class, ContextType>::Process(
                CallProcessorBinder* thisPtr, ICallContext* context)
            {
                if (context && context->RTTI_GetType() == ContextType::TYPEINFO_Uuid())
                {
                    return Call<Class, ContextType, Function>(thisPtr, context, m_function);
                }
                return ProcessingResult::Ignored;
            }

            //FunctionBindingTemplate
            template<typename Class, typename ContextType>
            CallProcessorBinder::FunctionBindingTemplate<Class, ContextType>::FunctionBindingTemplate(Function function)
                : m_function(function)
            {
            }

            template<typename Class, typename ContextType>
            ProcessingResult CallProcessorBinder::FunctionBindingTemplate<Class, ContextType>::Process(
                CallProcessorBinder* thisPtr, ICallContext* context)
            {
                if (context && context->RTTI_GetType() == ContextType::TYPEINFO_Uuid())
                {
                    return Call<Class, ContextType, Function>(thisPtr, context, m_function);
                }
                return ProcessingResult::Ignored;
            }

            // ConstDerivedFunctionBindingTemplate
            template<typename Class, typename ContextType>
            CallProcessorBinder::ConstDerivedFunctionBindingTemplate<Class, ContextType>::ConstDerivedFunctionBindingTemplate(Function function)
                : m_function(function)
            {
            }

            template<typename Class, typename ContextType>
            ProcessingResult CallProcessorBinder::ConstDerivedFunctionBindingTemplate<Class, ContextType>::Process(
                CallProcessorBinder* thisPtr, ICallContext* context)
            {
                if (context && context->RTTI_IsTypeOf(ContextType::TYPEINFO_Uuid()))
                {
                    return Call<Class, ContextType, Function>(thisPtr, context, m_function);
                }
                return ProcessingResult::Ignored;
            }

            //DerivedFunctionBindingTemplate
            template<typename Class, typename ContextType>
            CallProcessorBinder::DerivedFunctionBindingTemplate<Class, ContextType>::DerivedFunctionBindingTemplate(Function function)
                : m_function(function)
            {
            }

            template<typename Class, typename ContextType>
            ProcessingResult CallProcessorBinder::DerivedFunctionBindingTemplate<Class, ContextType>::Process(
                CallProcessorBinder* thisPtr, ICallContext* context)
            {
                if (context && context->RTTI_IsTypeOf(ContextType::TYPEINFO_Uuid()))
                {
                    return Call<Class, ContextType, Function>(thisPtr, context, m_function);
                }
                return ProcessingResult::Ignored;
            }
        } // namespace Events
    } // namespace SceneAPI
} // namespace AZ
