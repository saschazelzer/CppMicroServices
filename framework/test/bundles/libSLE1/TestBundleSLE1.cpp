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
#include <stdexcept>

#include "TestBundleSLE1Service.h"

#include "cppmicroservices/BundleActivator.h"
#include "cppmicroservices/BundleContext.h"
#include "cppmicroservices/GlobalConfig.h"

#include "cppmicroservices/SharedLibraryException.h"

namespace cppmicroservices {

struct TestBundleSLE1 : public TestBundleSLE1Service
{

  TestBundleSLE1() {}
  virtual ~TestBundleSLE1() {}
};

class TestBundleSLE1Activator : public BundleActivator
{
public:
  TestBundleSLE1Activator() {}
  ~TestBundleSLE1Activator() {}

  void Start(BundleContext)
  {
    //
    throw cppmicroservices::SharedLibraryException(std::error_code(), "test");
//    s = std::make_shared<TestBundleSLE1>();
//    std::cout << "Registering TestBundleSLE1Service";
//    sr = context.RegisterService<TestBundleSLE1Service>(s);
  }

  void Stop(BundleContext) {
//    sr.Unregister();
  }

//private:
//  std::shared_ptr<TestBundleSLE1> s;
//  ServiceRegistration<TestBundleSLE1Service> sr;
};
}

CPPMICROSERVICES_EXPORT_BUNDLE_ACTIVATOR(cppmicroservices::TestBundleSLE1Activator)
