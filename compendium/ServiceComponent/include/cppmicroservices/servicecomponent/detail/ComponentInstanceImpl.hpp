/*=============================================================================

  Library: CppMicroServices

  Copyright (c) The CppMicroServices developers. See the COPYRIGHT
  file at the top-level directory of this distribution and at
  https://github.com/CppMicroServices/CppMicroServices/COPYRIGHT .

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  =============================================================================*/

#ifndef ComponentWrapperImpl_hpp
#define ComponentWrapperImpl_hpp

#include <array>
#include <cassert>
#include <memory>
#include <tuple>
#include <vector>

#include "Binders.hpp"
#include "ComponentInstance.hpp"
#include "cppmicroservices/AnyMap.h"
#include "cppmicroservices/servicecomponent/ComponentContext.hpp"

namespace cppmicroservices
{
    namespace service
    {
        namespace component
        {
            namespace detail
            {

                /**
                 * Util class to detect if a class has a method named Activate
                 * Member Detector Idiom - https://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Member_Detector
                 */
                template <class T, class ReturnT, class... ArgsT>
                class HasActivate
                {
                  public:
                    struct BadType
                    {
                    };

                    template <class U>
                    static decltype(std::declval<U>().Activate(std::declval<ArgsT>()...)) Test(int);

                    template <class U>
                    static BadType Test(...);

                    static constexpr bool value = std::is_same<decltype(Test<T>(0)), ReturnT>::value;
                };

                /**
                 * Util class to detect if a class has a method named Modified
                 * Member Detector Idiom - https://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Member_Detector
                 */
                template <class T, class ReturnT, class... ArgsT>
                class HasModified
                {
                  public:
                    struct BadType
                    {
                    };

                    template <class U>
                    static decltype(std::declval<U>().Modified(std::declval<ArgsT>()...)) Test(int);

                    template <class U>
                    static BadType Test(...);

                    static constexpr bool value = std::is_same<decltype(Test<T>(0)), ReturnT>::value;
                };

                /**
                 * Util class to detect if a class has a method named Deactivate
                 * Member Detector Idiom - https://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Member_Detector
                 */
                template <class T, class ReturnT, class... ArgsT>
                class HasDeactivate
                {
                  public:
                    struct BadType
                    {
                    };

                    template <class U>
                    static decltype(std::declval<U>().Deactivate(std::declval<ArgsT>()...)) Test(int);

                    template <class U>
                    static BadType Test(...);

                    static constexpr bool value = std::is_same<decltype(Test<T>(0)), ReturnT>::value;
                };

                namespace util
                {

                    /**
                     * Helper function used to call a given function for each element of the tuple
                     */
                    template <typename T, typename FUNC, std::size_t... Is>
                    void
                    for_each(T&& t, FUNC f, std::index_sequence<Is...>)
                    {
                        // we use the comma operator to execute the function after the parameters are unpacked.
                        int dummy[] = { 0, (f(std::get<Is>(t), Is), 0)... };
                        static_cast<void>(dummy); // silence unused varible warning
                    }

                    template <std::size_t I, class T>
                    using tuple_element_t = typename std::tuple_element<I, T>::type;

                } // namespace util

                template <typename... Ts, typename FUNC>
                void
                for_each_in_tuple(std::tuple<Ts...> const& t, FUNC f)
                {
                    util::for_each(t, f, std::make_index_sequence<std::tuple_size<std::tuple<Ts...>>::value> {});
                }

                template <class T, class InterfaceTuple = std::tuple<>>
                class ComponentInstanceImplBase : public ComponentInstance
                {
                  public:
                    ComponentInstanceImplBase(std::vector<std::shared_ptr<Binder<T>>> const& binders = {})
                        : mContext(nullptr)
                        , refBinders(binders)
                    {
                        // move all binders into a map
                        for (size_t i = 0; i < refBinders.size(); i++)
                        {
                            refBinderMap[refBinders.at(i)->GetReferenceName()] = i;
                        }
                    }

                    virtual ~ComponentInstanceImplBase() = default;

                    void
                    Activate() override
                    {
                        DoActivate(mContext);
                    };

                    void
                    Deactivate() override
                    {
                        DoDeactivate(mContext);
                    };

                    /**
                     * This method is called by the runtime while the component configuration is active when
                     * the configuration properties are modified.
                     */
                    void
                    Modified() override
                    {
                        DoModified(mContext);
                    };

                    bool
                    DoesModifiedMethodExist() override
                    {
                        return DoDoesModifiedMethodExist();
                    }

                    void
                    InvokeBindMethod(std::string const& refName,
                                     cppmicroservices::ServiceReferenceBase const& sRef) override
                    {
                        size_t index = refBinderMap.at(refName);
                        refBinders.at(index)->Bind(mContext->GetBundleContext(), sRef, mServiceImpl);
                    };

                    void
                    InvokeUnbindMethod(std::string const& refName,
                                       cppmicroservices::ServiceReferenceBase const& sRef) override
                    {
                        size_t index = refBinderMap.at(refName);
                        refBinders.at(index)->UnBind(mContext->GetBundleContext(), sRef, mServiceImpl);
                    };

                    virtual std::shared_ptr<T>
                    GetInstance() const
                    {
                        return mServiceImpl;
                    };

                    cppmicroservices::InterfaceMapPtr
                    GetInterfaceMap() override
                    {
                        return GetInterfaceMapHelper(mServiceImpl);
                    }

                    template <class C = T, class I = InterfaceTuple, std::size_t... Is>
                    cppmicroservices::InterfaceMapPtr
                    MakeInterfaceMapWithTuple(std::shared_ptr<T> const& sObj, std::index_sequence<Is...>)
                    {
                        // The static_ptr_cast inside MakeInterfaceMap will fail if the sObj does not implement any of
                        // the interfaces in I.
                        InterfaceMapPtr iMap = MakeInterfaceMap<util::tuple_element_t<Is, I>...>(sObj);
                        assert(iMap->size() == std::tuple_size<I>::value);
                        return iMap;
                    }

                    template <class... Args>
                    cppmicroservices::InterfaceMapPtr
                    GetInterfaceMapHelper(Args...)
                    {
                        return nullptr;
                    }

                    template <class I = InterfaceTuple,
                              typename std::enable_if<std::tuple_size<I>::value != 0, int>::type = 0>
                    cppmicroservices::InterfaceMapPtr
                    GetInterfaceMapHelper(std::shared_ptr<T> const& sObj)
                    {
                        return MakeInterfaceMapWithTuple(sObj, std::make_index_sequence<std::tuple_size<I>::value> {});
                    }

                    /**
                     * This method is used if the component implementation class does not provide an Activate method.
                     */
                    template <typename... A>
                    void
                    DoActivate(A...)
                    {
                        // no-op
                    }

                    /**
                     * This method is used if the component implementation class provides an Activate method.
                     */
                    template <class Impl = T,
                              class HasActivateMethod = typename std::enable_if<
                                  HasActivate<Impl, void, std::shared_ptr<ComponentContext> const&>::value>::type>
                    void
                    DoActivate(std::shared_ptr<ComponentContext> const& ctxt)
                    {
                        if (mServiceImpl)
                        {
                            mServiceImpl->Activate(ctxt);
                        }
                    }

                    template <typename... A>
                    bool
                    DoModified(A...)
                    {
                        return false; // no modified method available
                    }

                    /**
                     * This method is used if the component implementation class provides a Modified method.
                     */
                    template <class Impl = T,
                              class HasModifiedMethod = typename std::enable_if<
                                  HasModified<Impl,
                                              void,
                                              std::shared_ptr<ComponentContext> const&,
                                              std::shared_ptr<cppmicroservices::AnyMap> const&>::value>::type>
                    bool
                    DoModified(std::shared_ptr<ComponentContext> const& ctxt)
                    {
                        if (!mServiceImpl)
                        {
                            return false;
                        }
                        auto properties = std::make_shared<cppmicroservices::AnyMap>(ctxt->GetProperties());
                        mServiceImpl->Modified(ctxt, properties);
                        return true;
                    }
                    /**
                     * This method is used to determine if the component implementation class provides a Modified
                     * method. This is used when no Modified method exists.
                     */
                    template <typename... A>
                    bool
                    DoDoesModifiedMethodExist(A...)
                    {
                        return false; // no modified method available
                    }

                    /**
                     * This method is used to determine if the component implementation class provides a Modified
                     * method. This method is used when a Modified method does exist.
                     */
                    template <class Impl = T,
                              class HasModifiedMethod = typename std::enable_if<
                                  HasModified<Impl,
                                              void,
                                              std::shared_ptr<ComponentContext> const&,
                                              std::shared_ptr<cppmicroservices::AnyMap> const&>::value>::type>
                    bool
                    DoDoesModifiedMethodExist()
                    {
                        return true;
                    }
                    /**
                     * This method is used if the component implementation class does not provide a Deactivate method.
                     */
                    template <typename... A>
                    void
                    DoDeactivate(A...)
                    {
                        // no-op
                    }

                    /**
                     * This method is used if the component implementation class provides a Deactivate method.
                     */
                    template <class Impl = T,
                              class HasDeactivateMethod = typename std::enable_if<
                                  HasDeactivate<Impl, void, std::shared_ptr<ComponentContext> const&>::value>::type>
                    void
                    DoDeactivate(std::shared_ptr<ComponentContext> const& ctxt)
                    {
                        if (mServiceImpl)
                        {
                            mServiceImpl->Deactivate(ctxt);
                        }
                    }

                    std::shared_ptr<ComponentContext>
                        mContext;                    // The context object associated with the component instance
                    std::shared_ptr<T> mServiceImpl; // Instance of the implementation class
                    std::vector<std::shared_ptr<Binder<T>>>
                        refBinders; // Helpers used for bind and unbind calls on the instance
                    std::map<std::string, size_t> refBinderMap; // a map to retrieve binders based on reference name
                };

                template <class T, class InterfaceTuple = std::tuple<>, class... CtorInjectedRefs>
                class ComponentInstanceImpl final : public ComponentInstanceImplBase<T, InterfaceTuple>
                {
                  public:
                    ComponentInstanceImpl(std::array<std::string, sizeof...(CtorInjectedRefs)> const staticRefNames
                                          = {},
                                          std::vector<std::shared_ptr<Binder<T>>> const& binders = {})
                        : ComponentInstanceImplBase<T, InterfaceTuple>(binders)
                        , mStaticRefNames(staticRefNames)
                    {
                    }

                    virtual ~ComponentInstanceImpl() = default;

                    using Injection = std::integral_constant<bool, sizeof...(CtorInjectedRefs) != 0>;

                    void
                    CreateInstance(std::shared_ptr<ComponentContext> const& ctxt) override
                    {
                        this->mContext = ctxt;
                        bool isConstructorInjected = Injection::value;
                        std::shared_ptr<T> implObj = DoCreate(isConstructorInjected); // appropriate
                        this->mServiceImpl = implObj;
                    }

                    void
                    BindReferences(std::shared_ptr<ComponentContext> const& ctxt) override
                    {
                        for (auto& binder : this->refBinders)
                        {
                            binder->Bind(ctxt, this->mServiceImpl);
                        }
                    }

                    void
                    UnbindReferences() override
                    {
                        for (auto& binder : this->refBinders)
                        {
                            binder->Unbind(this->mContext, this->mServiceImpl);
                        }
                    }

                    /**
                     * DoCreate is a helper function used to invoke the appropriate constructor on the Implementation
                     * class. SFINAE is used to determine which overload of DoCreate is used by the runtime.
                     */
                    template <typename... A>
                    std::shared_ptr<T>
                    DoCreate(A...)
                    {
                        // no-op fallback to mute the compiler.
                        // There is always an overload with higher precedence available.
                        return nullptr;
                    }

                    // this method is used when injection is false and default constructor is provided by the
                    // implementation class
                    template <class C = T,
                              class I = Injection,
                              class InjectionFalse = typename std::enable_if<I::value == false>::type,
                              class IsDefaultConstructible = typename std::enable_if<
                                  std::is_default_constructible<C>::value
                                  && !std::is_constructible<C, std::shared_ptr<cppmicroservices::AnyMap> const&>::
                                         value>::type>
                    std::shared_ptr<T>
                    DoCreate(bool)
                    {
                        return std::make_shared<T>();
                    }

                    // this method is used when injection is false and a constructor with Configuration properties input
                    // parameter is provided by the implementation class
                    template <class C = T,
                              class I = Injection,
                              class InjectionFalse = typename std::enable_if<I::value == false>::type,
                              class HasConfigConstructor = typename std::enable_if<
                                  std::is_constructible<C, std::shared_ptr<cppmicroservices::AnyMap> const&>::value
                                  == true>::type>
                    std::shared_ptr<T>
                    DoCreate(bool, bool = true)
                    {
                        return std::make_shared<T>(
                            std::make_shared<cppmicroservices::AnyMap>(this->mContext->GetProperties()));
                    }

                    // this method is used when injection is false and neither a default constructor nor a constructor
                    // with a Configuration properties input parameter is provided by the implementation class
                    template <class C = T,
                              class I = Injection,
                              class InjectionFalse = typename std::enable_if<I::value == false>::type,
                              class HasNoDefaultConstructor
                              = typename std::enable_if<std::is_default_constructible<C>::value == false>::type,
                              class HasNoConfigConstructor = typename std::enable_if<
                                  std::is_constructible<C, std::shared_ptr<cppmicroservices::AnyMap> const&>::value
                                  == false>::type>
                    std::shared_ptr<T>
                    DoCreate(bool, bool = true, bool = true)
                    {
                        static_assert(std::is_default_constructible<C>::value,
                                      "An appropriate constructor was not found and/or the service implementation does "
                                      "not implement all of the service interface's methods. "
                                      "A default constructor or a constructor with an AnyMap input parameter for the "
                                      "configuration properties is required when inject-references is false. "
                                      "A properly defined Modified, Activate, and Deactivate method are required when "
                                      "inheriting from UserDefinedMethodVerification.");
                        return nullptr;
                    }

                    // this method is used when injection is true and constructor with suitable parameters is not
                    // provided by the implementation class
                    template <class C = T,
                              class I = Injection,
                              class InjectionTrue = typename std::enable_if<I::value == true>::type,
                              class HasNoConstructorWithReferences = typename std::enable_if<
                                  std::is_constructible<C, CtorInjectedRefs const&...>::value == false>::type,
                              class HasNoConstructorWithRefAndConfig = typename std::enable_if<
                                  std::is_constructible<C,
                                                        std::shared_ptr<cppmicroservices::AnyMap> const&,
                                                        CtorInjectedRefs const&...>::value
                                  == false>::type>
                    std::shared_ptr<T>
                    DoCreate(bool const&)
                    {
                        static_assert(std::is_constructible<C, CtorInjectedRefs const&...>::value,
                                      "An appropriate constructor was not found "
                                      "and/or the service implementation does not implement all of the "
                                      "service interface's methods. A constructor with service reference "
                                      "input parameters or a constructor with an AnyMap input parameter for "
                                      "configuration properties and service reference input parameters is "
                                      "required when inject-references is true. A properly defined Modified, Activate, "
                                      "and Deactivate method are required when "
                                      "inheriting from UserDefinedMethodVerification.");
                        return nullptr;
                    }

                    // this method is used when injection is true and constructor with reference parameters is provided
                    // by the implementation class
                    template <class C = T,
                              class I = Injection,
                              class InjectionTrue = typename std::enable_if<I::value == true>::type,
                              class HasConstructorWithReferences = typename std::enable_if<
                                  std::is_constructible_v<C, CtorInjectedRefs const&...>
                                  || std::is_constructible_v<C, CtorInjectedRefs const&&...>>::type>
                    std::shared_ptr<T>
                    DoCreate(bool& injected)
                    {
                        std::tuple<CtorInjectedRefs...> depObjs = GetAllDependencies(
                            std::make_index_sequence<std::tuple_size<std::tuple<CtorInjectedRefs...>>::value> {});
                        std::shared_ptr<T> implObj = call_make_shared_with_tuple(
                            std::move(depObjs),
                            std::make_index_sequence<std::tuple_size<std::tuple<CtorInjectedRefs...>>::value> {});
                        injected = (implObj != nullptr);
                        return implObj;
                    }

                    // this method is used when injection is true and constructor with reference parameters and the
                    // configuration property parameter is provided by the implementation class
                    template <class C = T,
                              class I = Injection,
                              class InjectionTrue = typename std::enable_if<I::value == true>::type,
                              class HasConstructorWithRefAndConfig = typename std::enable_if<
                                  std::is_constructible_v<C,
                                                          std::shared_ptr<cppmicroservices::AnyMap> const&,
                                                          CtorInjectedRefs const&...>
                                  || std::is_constructible_v<C,
                                                             std::shared_ptr<cppmicroservices::AnyMap> const&,
                                                             CtorInjectedRefs const&&...>>::type>
                    std::shared_ptr<T>
                    DoCreate(bool& injected, bool = true)
                    {
                        std::tuple<CtorInjectedRefs...> depObjs = GetAllDependencies(
                            std::make_index_sequence<std::tuple_size<std::tuple<CtorInjectedRefs...>>::value> {});
                        auto props = std::make_shared<cppmicroservices::AnyMap>(this->mContext->GetProperties());
                        std::shared_ptr<T> implObj = call_make_shared_with_tuple_and_props(
                            props,
                            std::move(depObjs),
                            std::make_index_sequence<std::tuple_size<std::tuple<CtorInjectedRefs...>>::value> {});
                        injected = (implObj != nullptr);
                        return implObj;
                    }

                    template <std::size_t... Is>
                    std::shared_ptr<T>
                    call_make_shared_with_tuple(std::tuple<CtorInjectedRefs...> const& tuple,
                                                std::index_sequence<Is...>)
                    {
                        return std::make_shared<T>(std::get<Is>(tuple)...);
                    }

                    template <std::size_t... Is>
                    std::shared_ptr<T>
                    call_make_shared_with_tuple(std::tuple<std::decay_t<CtorInjectedRefs>...> const&& tuple,
                                                std::index_sequence<Is...>)
                    {
                        return std::make_shared<T>(std::forward<CtorInjectedRefs const&&>(std::get<Is>(tuple))...);
                    }

                    template <std::size_t... Is>
                    std::shared_ptr<T>
                    call_make_shared_with_tuple_and_props(std::shared_ptr<cppmicroservices::AnyMap> const& props,
                                                          std::tuple<CtorInjectedRefs const&...> const& tuple,
                                                          std::index_sequence<Is...>)
                    {
                        return std::make_shared<T>(props, std::get<Is>(tuple)...);
                    }

                    template <std::size_t... Is>
                    std::shared_ptr<T>
                    call_make_shared_with_tuple_and_props(
                        std::shared_ptr<cppmicroservices::AnyMap> const& props,
                        std::tuple<std::decay_t<CtorInjectedRefs const&>...> const&& tuple,
                        std::index_sequence<Is...>)
                    {
                        return std::make_shared<T>(props,
                                                   std::forward<CtorInjectedRefs const&&>(std::get<Is>(tuple))...);
                    }

                    // Type detector to see if given template parameter is std::vector type (multiple cardinality
                    // reference) or not (unary cardinality reference)
                    template <typename RefType>
                    struct is_vector_type
                    {
                        static bool const value = false;
                    };

                    template <typename RefType>
                    struct is_vector_type<std::vector<RefType>>
                    {
                        static bool const value = true;
                    };

                    template <std::size_t... Is>
                    std::tuple<CtorInjectedRefs...>
                    GetAllDependencies(std::index_sequence<Is...>)
                    {
                        return std::make_tuple(
                            GetDependency<typename std::tuple_element<Is, std::tuple<CtorInjectedRefs...>>::type>(
                                std::get<Is>(mStaticRefNames))...);
                    }

                    template <class R,
                              class IsVectorType = typename std::enable_if<is_vector_type<R>::value == true>::type>
                    R
                    GetDependency(std::string const& name)
                    {
                        // Overload to be invoked for references using multiple cardinality
                        using VectorElementType = typename R::value_type;
                        using RefType = typename VectorElementType::element_type;
                        return this->mContext->template LocateServices<RefType>(name);
                    }

                    template <class R,
                              class IsNotVectorType = typename std::enable_if<is_vector_type<R>::value == false>::type>
                    R
                    GetDependency(std::string const& name, bool = true)
                    {
                        // Overload to be used for references using unary cardinality
                        using RefType = typename R::element_type;
                        return this->mContext->template LocateService<RefType>(name);
                    }

                  private:
                    std::array<std::string, sizeof...(CtorInjectedRefs)> mStaticRefNames;
                };

            } // namespace detail
        } // namespace component
    } // namespace service
} // namespace cppmicroservices

#endif /* ComponentInstance_hpp */
