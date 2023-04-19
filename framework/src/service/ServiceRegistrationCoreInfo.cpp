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

#include "ServiceRegistrationCoreInfo.h"

#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4355)
#endif

namespace cppmicroservices
{

    ServiceRegistrationCoreInfo::ServiceRegistrationCoreInfo(BundlePrivate* bundle_,
                                                             InterfaceMapConstPtr service,
                                                             Properties&& props)
        : service(std::move(service))
        , bundle(bundle_->shared_from_this())
        , properties(std::move(props))
        , available(true)
        , unregistering(false)
    {
    }

    ServiceRegistrationCoreInfo::~ServiceRegistrationCoreInfo() = default;
} // namespace cppmicroservices

#ifdef _MSC_VER
#    pragma warning(pop)
#endif
