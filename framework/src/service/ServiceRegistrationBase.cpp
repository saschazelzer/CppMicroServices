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

#include "cppmicroservices/ServiceRegistrationBase.h"

#include "cppmicroservices/Bundle.h"
#include "cppmicroservices/FrameworkEvent.h"
#include "cppmicroservices/ServiceFactory.h"

#include "BundlePrivate.h"
#include "CoreBundleContext.h"
#include "ServiceListenerEntry.h"
#include "ServiceRegistrationBasePrivate.h"
#include "ServiceRegistrationLocks.h"
#include "ServiceRegistry.h"

#include <stdexcept>

US_MSVC_DISABLE_WARNING(4503) // decorated name length exceeded, name was truncated

namespace cppmicroservices
{

    ServiceRegistrationBase::ServiceRegistrationBase() = default;

    ServiceRegistrationBase::ServiceRegistrationBase(ServiceRegistrationBase const& reg) = default;

    ServiceRegistrationBase::ServiceRegistrationBase(ServiceRegistrationBase&& reg) noexcept = default;

    ServiceRegistrationBase::ServiceRegistrationBase(
        std::shared_ptr<ServiceRegistrationBasePrivate> registrationPrivate)
        : d(std::move(registrationPrivate))
    {
    }

    ServiceRegistrationBase::ServiceRegistrationBase(BundlePrivate* bundle,
                                                     InterfaceMapConstPtr const& service,
                                                     Properties&& props)
        : d(std::make_shared<ServiceRegistrationBasePrivate>(bundle, service, std::move(props)))
    {
        // Constructor of ServiceRegistrationBasePrivate does not take in the reference back to the
        // ServiceRegistrationBasePrivate. It is created after construction in CreateReference because the sharedPtr
        // back to ServiceRegistrationBasePrivate must be fully constructecd before shared_from_this can be called.
        d->CreateReference();
    }

    ServiceRegistrationBase::operator bool() const { return d != nullptr; }

    ServiceRegistrationBase&
    ServiceRegistrationBase::operator=(std::nullptr_t)
    {
        d = nullptr;
        return *this;
    }

    ServiceRegistrationBase::~ServiceRegistrationBase() = default;

    ServiceReferenceBase
    ServiceRegistrationBase::GetReference(std::string const& interfaceId) const
    {
        if (!d)
        {
            throw std::logic_error("ServiceRegistrationBase object invalid");
        }
        if (!d->coreInfo->available)
        {
            throw std::logic_error("Service is unregistered");
        }

        auto l = LockServiceRegistration();
        US_UNUSED(l);
        ServiceReferenceBase ref = d->reference;
        ref.SetInterfaceId(interfaceId);
        return ref;
    }

    void
    ServiceRegistrationBase::SetProperties(ServiceProperties const& props)
    {
        auto propsCopy(props);
        SetProperties(std::move(propsCopy));
    }

    void
    ServiceRegistrationBase::SetProperties(ServiceProperties&& propsCopy)
    {
        if (!d)
        {
            throw std::logic_error("ServiceRegistrationBase object invalid");
        }

        ServiceEvent modifiedEndMatchEvent;
        ServiceEvent modifiedEvent;

        ServiceListeners::ServiceListenerEntries before;

        if (!d->coreInfo->available)
        {
            throw std::logic_error("Service is unregistered");
        }

        {
            auto l = LockServiceRegistration();
            US_UNUSED(l);
            if (!d->coreInfo->available)
            {
                throw std::logic_error("Service is unregistered");
            }
            modifiedEndMatchEvent = ServiceEvent(ServiceEvent::SERVICE_MODIFIED_ENDMATCH, d->reference);
            modifiedEvent = ServiceEvent(ServiceEvent::SERVICE_MODIFIED, d->reference);
        }

        // This calls into service event listener hooks. We must not hold any locks here
        if (auto bundle = d->coreInfo->bundle_.lock())
        {
            bundle->coreCtx->listeners.GetMatchingServiceListeners(modifiedEndMatchEvent, before);
        }

        int old_rank = 0;
        int new_rank = 0;
        Any objectClasses;
        {
            auto l = LockServiceRegistration();
            US_UNUSED(l);
            if (!d->coreInfo->available)
            {
                throw std::logic_error("Service is unregistered");
            }

            auto l2 = d->coreInfo->properties.Lock();
            US_UNUSED(l2);

            propsCopy[Constants::SERVICE_ID]
                = std::move(d->coreInfo->properties.Value_unlocked(Constants::SERVICE_ID).first);
            objectClasses = std::move(d->coreInfo->properties.Value_unlocked(Constants::OBJECTCLASS).first);
            propsCopy[Constants::OBJECTCLASS] = objectClasses;
            propsCopy[Constants::SERVICE_SCOPE]
                = std::move(d->coreInfo->properties.Value_unlocked(Constants::SERVICE_SCOPE).first);

            auto itr = propsCopy.find(Constants::SERVICE_RANKING);
            if (itr != propsCopy.end())
            {
                try
                {
                    new_rank = any_cast<int>(itr->second);
                }
                catch (BadAnyCastException const& ex)
                {
                    std::string exMsg("SERVICE_RANKING property has unexpected value type. ");
                    exMsg.append(ex.what());
                    throw std::invalid_argument(exMsg);
                }
            }

            auto oldRankAny = d->coreInfo->properties.Value_unlocked(Constants::SERVICE_RANKING).first;
            if (!oldRankAny.Empty())
            {
                // since the old ranking is extracted from existing service properties
                // stored in the service registry, no need to type check before casting
                old_rank = any_cast<int>(oldRankAny);
            }
            d->coreInfo->properties = Properties(AnyMap(std::move(propsCopy)));
        }
        if (old_rank != new_rank)
        {
            auto const& classes = ref_any_cast<std::vector<std::string>>(objectClasses);
            if (auto bundle = d->coreInfo->bundle_.lock())
            {
                bundle->coreCtx->services.UpdateServiceRegistrationOrder(classes);
            }
        }

        // Notify listeners, we must not hold any locks here
        ServiceListeners::ServiceListenerEntries matchingListeners;
        if (auto bundle = d->coreInfo->bundle_.lock())
        {
            bundle->coreCtx->listeners.GetMatchingServiceListeners(modifiedEvent, matchingListeners);
            bundle->coreCtx->listeners.ServiceChanged(matchingListeners, modifiedEvent, before);
            bundle->coreCtx->listeners.ServiceChanged(before, modifiedEndMatchEvent);
        }
    }

    void
    ServiceRegistrationBase::Unregister()
    {
        if (!d)
        {
            throw std::logic_error("ServiceRegistrationBase object invalid");
        }

        CoreBundleContext* coreContext = nullptr;

        bool isUnregistering(false); // expected state

        if (!atomic_compare_exchange_strong(&d->coreInfo->unregistering, &isUnregistering, true))
        {
            // someone else is unregistering
            return;
        }

        // d->coreInfo->unregistering is now true

        // if unavailable
        if (!d->coreInfo->available)
        {
            // set unregistering to false
            d->coreInfo->unregistering = false;
            throw std::logic_error("Service is unregistered");
        }

        if (auto bundle = d->coreInfo->bundle_.lock())
        {
            {
                auto l1 = bundle->coreCtx->services.Lock();
                US_UNUSED(l1);
                bundle->coreCtx->services.RemoveServiceRegistration_unlocked(*this);
            }
            coreContext = bundle->coreCtx;
        }

        if (coreContext)
        {
            // Notify listeners. We must not hold any locks here.
            ServiceListeners::ServiceListenerEntries listeners;
            ServiceEvent unregisteringEvent(ServiceEvent::SERVICE_UNREGISTERING, d->reference);
            coreContext->listeners.GetMatchingServiceListeners(unregisteringEvent, listeners);
            coreContext->listeners.ServiceChanged(listeners, unregisteringEvent);
        }

        std::shared_ptr<ServiceFactory> serviceFactory;
        ServiceRegistrationBasePrivate::BundleToServicesMap prototypeServiceInstances;
        ServiceRegistrationBasePrivate::BundleToServiceMap bundleServiceInstance;

        {
            auto l = LockServiceRegistration();
            US_UNUSED(l);
            d->coreInfo->available = false;
            auto factoryIter = d->coreInfo->service->find("org.cppmicroservices.factory");
            if (auto bundle = d->coreInfo->bundle_.lock() && factoryIter != d->coreInfo->service->end())
            {
                if (bundle)
                {
                    serviceFactory = std::static_pointer_cast<ServiceFactory>(factoryIter->second);
                }
            }
            if (serviceFactory)
            {
                prototypeServiceInstances = d->coreInfo->prototypeServiceInstances;
                bundleServiceInstance = d->coreInfo->bundleServiceInstance;
            }
        }

        if (serviceFactory)
        {
            // unget all prototype services
            for (auto const& i : prototypeServiceInstances)
            {
                for (auto const& service : i.second)
                {
                    try
                    {
                        serviceFactory->UngetService(MakeBundle(i.first->shared_from_this()), *this, service);
                    }
                    catch (std::exception const& ex)
                    {
                        std::string message("ServiceFactory UngetService implementation threw an exception");
                        if (auto bundle = d->coreInfo->bundle_.lock())
                        {
                            bundle->coreCtx->listeners.SendFrameworkEvent(FrameworkEvent(
                                FrameworkEvent::Type::FRAMEWORK_ERROR,
                                MakeBundle(bundle->shared_from_this()),
                                message,
                                std::make_exception_ptr(
                                    ServiceException(ex.what(), ServiceException::Type::FACTORY_EXCEPTION))));
                        }
                    }
                }
            }

            // unget bundle scope services
            for (auto const& i : bundleServiceInstance)
            {
                try
                {
                    serviceFactory->UngetService(MakeBundle(i.first->shared_from_this()), *this, i.second);
                }
                catch (std::exception const& ex)
                {
                    std::string message("ServiceFactory UngetService implementation threw an exception");
                    if (auto bundle = d->coreInfo->bundle_.lock())
                    {
                        bundle->coreCtx->listeners.SendFrameworkEvent(FrameworkEvent(
                            FrameworkEvent::Type::FRAMEWORK_ERROR,
                            MakeBundle(bundle->shared_from_this()),
                            message,
                            std::make_exception_ptr(
                                ServiceException(ex.what(), ServiceException::Type::FACTORY_EXCEPTION))));
                    }
                }
            }
        }

        {
            auto l = LockServiceRegistration();
            US_UNUSED(l);

            d->coreInfo->bundle_.reset();
            d->coreInfo->dependents.clear();
            d->coreInfo->service.reset();
            d->coreInfo->prototypeServiceInstances.clear();
            d->coreInfo->bundleServiceInstance.clear();

            d->reference = nullptr;

            // reset d->coreInfo->unregistering to false
            d->coreInfo->unregistering = false;
        }
    }

    ServiceRegistrationLocks
    ServiceRegistrationBase::LockServiceRegistration() const
    {
        return ServiceRegistrationLocks(d, d->coreInfo);
    }

    bool
    ServiceRegistrationBase::operator<(ServiceRegistrationBase const& o) const
    {
        if (this == &o || d == o.d)
        {
            return false;
        }

        if ((!d && !o.d) || !o.d)
        {
            return false;
        }
        if (!d)
        {
            return true;
        }

        ServiceReferenceBase sr1 = (LockServiceRegistration(), d->reference);
        ServiceReferenceBase sr2 = (o.LockServiceRegistration(), o.d->reference);

        return sr1 < sr2;
    }

    bool
    ServiceRegistrationBase::operator==(ServiceRegistrationBase const& registration) const
    {
        return d == registration.d;
    }

    ServiceRegistrationBase& ServiceRegistrationBase::operator=(ServiceRegistrationBase const& registration) = default;

    ServiceRegistrationBase& ServiceRegistrationBase::operator=(ServiceRegistrationBase&& registration) noexcept
        = default;

    std::ostream&
    operator<<(std::ostream& os, ServiceRegistrationBase const&)
    {
        return os << "cppmicroservices::ServiceRegistrationBase object";
    }
} // namespace cppmicroservices
