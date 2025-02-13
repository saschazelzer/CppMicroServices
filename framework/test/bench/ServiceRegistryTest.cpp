#include "benchmark/benchmark.h"
#include "cppmicroservices/ServiceEvent.h"
#include <cppmicroservices/Bundle.h>
#include <cppmicroservices/BundleContext.h>
#include <cppmicroservices/BundleEvent.h>
#include <cppmicroservices/Constants.h>
#include <cppmicroservices/Framework.h>
#include <cppmicroservices/FrameworkEvent.h>
#include <cppmicroservices/FrameworkFactory.h>
#include <cppmicroservices/ServiceFactory.h>
#include <cppmicroservices/ServiceObjects.h>

#include <chrono>
#include <iostream>
#include <vector>

using namespace cppmicroservices;

namespace
{
    /*
     * Interface used for Registering services
     */
    class TestInterface
    {
    };

    class ServiceRegistryFixture : public ::benchmark::Fixture
    {
      public:
        using benchmark::Fixture::SetUp;
        using benchmark::Fixture::TearDown;

        void
        SetUp(::benchmark::State const&)
        {
            framework = std::make_shared<Framework>(FrameworkFactory().NewFramework());
            framework->Start();
        }

        void
        TearDown(::benchmark::State const&)
        {
            framework->Stop();
            framework->WaitForStop(std::chrono::milliseconds::zero());
        }

        ~ServiceRegistryFixture() { framework.reset(); };

        std::shared_ptr<Framework> framework;
    };

} // namespace

/**
 * Utility method to construct an interface map. The map returned by this method
 * must not be used with the template versions of RegisterService & GetServiceReference
 */
InterfaceMapPtr
MakeInterfaceMapWithNInterfaces(int64_t interfaceCount)
{
    auto impl = std::make_shared<TestInterface>();
    InterfaceMapPtr iMap = MakeInterfaceMap<>(impl);
    iMap->clear();
    for (auto j = interfaceCount; j > 0; --j)
    {
        std::string iName { "TestInterface" + std::to_string(j) };
        iMap->insert(std::make_pair(iName, impl));
    }
    return iMap;
}

BENCHMARK_DEFINE_F(ServiceRegistryFixture, RegisterServices)
(benchmark::State& state)
{
    using namespace std::chrono;

    auto fc = framework->GetBundleContext();
    auto regCount = state.range(0);
    auto interfaceCount = state.range(1);
    auto interfaceMap = MakeInterfaceMapWithNInterfaces(interfaceCount);

    for (auto _ : state)
    {
        for (auto i = regCount; i > 0; --i)
        {
            InterfaceMapPtr iMapCopy(std::make_shared<InterfaceMap>(*interfaceMap));
            auto start = high_resolution_clock::now();
            auto reg = fc.RegisterService(iMapCopy); // benchmark the call to RegisterService
            auto end = high_resolution_clock::now();
            US_UNUSED(reg);
            auto elapsed_seconds = duration_cast<duration<double>>(end - start);
            state.SetIterationTime(elapsed_seconds.count());
        }
    }
}

// first parameter in Ranges specifies the number of calls to RegisterService
// second parameter in the Ranges specifies the number of interfaces used in the call to RegisterService
BENCHMARK_REGISTER_F(ServiceRegistryFixture, RegisterServices)
    ->RangeMultiplier(4)
    ->Ranges({
        {1, 1000},
        {1, 1000}
})
    ->UseManualTime();

BENCHMARK_DEFINE_F(ServiceRegistryFixture, RegisterServicesWithRank)
(benchmark::State& state)
{
    auto fc = framework->GetBundleContext();
    auto regCount = state.range(0);
    auto interfaceCount = state.range(1);
    auto interfaceMap = MakeInterfaceMapWithNInterfaces(interfaceCount);

    for (auto _ : state)
    {
        for (auto i = regCount; i > 0; --i)
        {
            InterfaceMapPtr iMapCopy(std::make_shared<InterfaceMap>(*interfaceMap));
            auto start = std::chrono::high_resolution_clock::now();
            auto reg = fc.RegisterService(iMapCopy,
                                          ServiceProperties({
                                              {Constants::SERVICE_RANKING,
                                               Any(static_cast<int>(i))}
            })); // benchmark the call to RegisterService
            auto end = std::chrono::high_resolution_clock::now();
            US_UNUSED(reg);
            auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
            state.SetIterationTime(elapsed_seconds.count());
        }
    }
}

// first parameter in Ranges specifies the number of calls to RegisterService
// second parameter in the Ranges specifies the number of interfaces used in the call to RegisterService
BENCHMARK_REGISTER_F(ServiceRegistryFixture, RegisterServicesWithRank)
    ->RangeMultiplier(4)
    ->Ranges({
        {1, 1000},
        {1, 1000}
})
    ->UseManualTime();

BENCHMARK_DEFINE_F(ServiceRegistryFixture, FindServices)
(benchmark::State& state)
{
    auto fc = framework->GetBundleContext();
    auto regCount = state.range(0);
    auto interfaceCount = state.range(1);
    auto interfaceMap = MakeInterfaceMapWithNInterfaces(interfaceCount);
    std::vector<ServiceRegistrationU> regs;

    for (auto i = regCount; i > 0; --i)
    {
        InterfaceMapPtr iMapCopy(std::make_shared<InterfaceMap>(*interfaceMap));
        regs.emplace_back(fc.RegisterService(iMapCopy));
    }

    for (auto _ : state)
    {
        for (auto iPair : *interfaceMap)
        {
            auto sRef = fc.GetServiceReference(iPair.first);
            auto service = fc.GetService(sRef);
            (void)service; // unused service object
        }
    }
}

// first parameter in Ranges specifies the number of calls to RegisterService
// second parameter in the Ranges specifies the number of interfaces used in the call to RegisterService
BENCHMARK_REGISTER_F(ServiceRegistryFixture, FindServices)
    ->RangeMultiplier(4)
    ->Ranges({
        {1, 1000},
        {1, 1000}
});

BENCHMARK_DEFINE_F(ServiceRegistryFixture, UnregisterServices)
(benchmark::State& state)
{
    auto fc = framework->GetBundleContext();
    auto regCount = state.range(0);
    auto interfaceCount = state.range(1);
    auto interfaceMap = MakeInterfaceMapWithNInterfaces(interfaceCount);

    for (auto _ : state)
    {
        std::vector<ServiceRegistrationBase> regs;
        for (auto i = regCount; i > 0; --i)
        {
            InterfaceMapPtr iMapCopy(std::make_shared<InterfaceMap>(*interfaceMap));
            auto reg = fc.RegisterService(iMapCopy); // benchmark the call to RegisterService
            regs.push_back(reg);
        }
        for (auto& reg : regs)
        {
            auto start = std::chrono::high_resolution_clock::now();
            reg.Unregister();
            auto end = std::chrono::high_resolution_clock::now();
            auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
            state.SetIterationTime(elapsed_seconds.count());
        }
    }
}

// first parameter in Ranges specifies the number of calls to RegisterService
// second parameter in the Ranges specifies the number of interfaces used in the call to RegisterService
BENCHMARK_REGISTER_F(ServiceRegistryFixture, UnregisterServices)
    ->RangeMultiplier(4)
    ->Ranges({
        {1, 1000},
        {1, 1000}
})
    ->UseManualTime();

BENCHMARK_DEFINE_F(ServiceRegistryFixture, ModifyServices)
(benchmark::State& state)
{
    using namespace std::chrono;

    auto fc = framework->GetBundleContext();
    auto regCount = state.range(0);
    auto interfaceCount = state.range(1);
    auto interfaceMap = MakeInterfaceMapWithNInterfaces(interfaceCount);

    std::vector<ServiceRegistrationBase> regs;
    for (auto i = regCount; i > 0; --i)
    {
        InterfaceMapPtr iMapCopy(std::make_shared<InterfaceMap>(*interfaceMap));
        auto reg = fc.RegisterService(iMapCopy);
        regs.push_back(reg);
    }

    for (auto _ : state)
    {

        ServiceProperties props;
        props["perf.service.value"] = rand() % 100;

        auto start = high_resolution_clock::now();

        for (std::size_t i = 0; i < regs.size(); i++)
        {
            regs[i].SetProperties(props);
        }

        auto end = high_resolution_clock::now();
        auto elapsed_seconds = duration_cast<duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
    }
}

BENCHMARK_REGISTER_F(ServiceRegistryFixture, ModifyServices)
    ->RangeMultiplier(4)
    ->Ranges({
        {1, 1000},
        {1, 1000}
})
    ->UseManualTime();
