#ifndef _SERVICE_IMPL_HPP_
#define _SERVICE_IMPL_HPP_

#include "TestInterfaces/Interfaces.hpp"
#include "cppmicroservices/servicecomponent/ComponentContext.hpp"

using ComponentContext = cppmicroservices::service::component::ComponentContext;

namespace sample {
class ServiceComponent9 : public test::LifeCycleValidation
{
public:
  ServiceComponent9() = default;
  ~ServiceComponent9() override = default;
  void Activate(const std::shared_ptr<ComponentContext>& context);
  void Deactivate(const std::shared_ptr<ComponentContext>& context);
  bool IsActivated() override { return activated; };
  bool IsDeactivated() override { return deactivated; };

private:
  bool activated;
  bool deactivated;
};
}

#endif // _SERVICE_IMPL_HPP_
