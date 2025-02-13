
.. rubric:: Continuous Integration Status

+-------------+---------------------------+--------------------------------------+----------------------------------------+
| Branch      | GCC 7.5.0 and 9.4.0       | Visual Studio 2019                   |                                        |
|             +---------------------------+--------------------------------------+----------------------------------------+
|             | Clang 9.0                 | Visual Studio 2022                   |                                        |
|             +---------------------------+--------------------------------------+----------------------------------------+
|             | Xcode 15.2                | MinGW-w64                            |                                        |
|             +---------------------------+--------------------------------------+----------------------------------------+
|             | Xcode 15.4                |                                      |                                        |
+=============+===========================+======================================+========================================+
| master      | |BuildAndTestNix(master)| | |BuildAndTestWindows(master)|        | |Code Coverage Status|                 |
+-------------+---------------------------+--------------------------------------+----------------------------------------+
| development | |BuildAndTestNix|         | |BuildAndTestWindows|                | |Code Coverage Status (development)|   |
+-------------+---------------------------+--------------------------------------+----------------------------------------+

|Coverity Scan Build Status|

|Performance Status|
 

C++ Micro Services
==================

|RTD Build Status (stable)| |RTD Build Status (development)|

`Download <https://github.com/CppMicroServices/CppMicroServices/releases>`_

Introduction
------------

The C++ Micro Services project is a collection of components for building
modular and dynamic service-oriented applications. It is based on
`OSGi <http://osgi.org>`_, but tailored to support native cross-platform solutions.

Proper usage of C++ Micro Services patterns and concepts leads to systems
with one or more of the following properties:

- Re-use of software components
- Loose coupling between service providers and consumers
- Separation of concerns, based on a service-oriented design
- Clean APIs based on service interfaces
- Extensible and reconfigurable systems


Requirements
------------

None, except a recent enough C++ compiler. All third-party library
dependencies are included and mostly used for implementation details.

Supported Platforms
-------------------

The library makes use of C++17 language and library features and compiles
on many different platforms.

Recommended absolute minimum required compiler versions:

- GCC 7.5.0
- Clang 9.0
- Clang from Xcode 10.0 (not tested)
- Visual Studio 2017 (MSVC++ 15.0) (not tested)

Not all of the absolute minimum compiler versions are tested (as noted). We test and recommend
the following compilers:

- GCC 11.3.0
- Clang 11.0
- Clang from Xcode 15.2 and 15.4
- Visual Studio 2019 and 2022

Recommended minimum required CMake version:

- CMake 3.17.0

For all CI builds through GitHub Actions, the CMake version (and
version of other provided software) we use is determined by the 
software provided on the GitHub-hosted runners.

For information about the specific versions of software the runners
use, please see the following resources:

- `ubuntu-22.04 Runner Information <https://github.com/actions/runner-images/blob/main/images/ubuntu/Ubuntu2204-Readme.md>`_
- `macos-13 Runner Information <https://github.com/actions/runner-images/blob/main/images/macos/macos-13-Readme.md>`_
- `macos-14 Runner Information <https://github.com/actions/runner-images/blob/main/images/macos/macos-14-Readme.md>`_
- `windows-2019 Runner Information <https://github.com/actions/runner-images/blob/main/images/windows/Windows2019-Readme.md>`_
- `windows-2022 Runner Information <https://github.com/actions/runner-images/blob/main/images/windows/Windows2022-Readme.md>`_

Below is a list of tested compiler/OS combinations:

- GCC 11.3.0 (Ubuntu 22.04)
- Clang 14.0.0 (Ubuntu 22.04)
- Apple Clang from Xcode 15.2.0 (OS X 13.3.0)
- Apple Clang from Xcode 15.4.0 (OS X 14.5.0)
- Visual Studio 2019
- Visual Studio 2022
- MinGW-w64

Legal
-----

The C++ Micro Services project was initially developed at the German
Cancer Research Center. Its source code is hosted as a `GitHub Project`_.
See the `COPYRIGHT file`_ in the top-level directory for detailed
copyright information.

This project is licensed under the `Apache License v2.0`_.

Code of Conduct
---------------

CppMicroServices.org welcomes developers with different backgrounds and
a broad range of experience. A diverse and inclusive community will
create more great ideas, provide more unique perspectives, and produce
more outstanding code. Our aim is to make the CppMicroServices community
welcoming to everyone.

To provide clarity of what is expected of our members, CppMicroServices
has adopted the code of conduct defined by
`contributor-covenant.org <http://contributor-covenant.org>`_. This
document is used across many open source communities, and we believe it
articulates our values well.

Please refer to the :any:`Code of Conduct <code-of-conduct>` for further
details.

Quick Start
-----------

Start by cloning the project repository. It is important to note that since
the project utilizes git submodules, you must clone the repository with the
`--recursive` flag. This will also clone the submodules and place them in
their respective directories. For further reading about how git submodules
work and how to clone them into an already existing repository on your disk,
please see `Git's documentation <https://git-scm.com/book/en/v2/Git-Tools-Submodules>`_.

Essentially, the C++ Micro Services library provides you with a powerful
dynamic service registry on top of a managed lifecycle. The framework manages,
among other things, logical units of modularity called *bundles* that
are contained in shared or static libraries. Each bundle
within a library has an associated :any:`cppmicroservices::BundleContext`
object, through which the service registry is accessed.

To query the registry for a service object implementing one or more
specific interfaces, the code would look like this:

.. code:: cpp

    #include "cppmicroservices/BundleContext.h"
    #include "SomeInterface.h"

    using namespace cppmicroservices;

    void UseService(BundleContext context)
    {
      auto serviceRef = context.GetServiceReference<SomeInterface>();
      if (serviceRef)
      {
        auto service = context.GetService(serviceRef);
        if (service) { /* do something */ }
      }
    }

Registering a service object against a certain interface looks like
this:

.. code:: cpp

    #include "cppmicroservices/BundleContext.h"
    #include "SomeInterface.h"

    using namespace cppmicroservices;

    void RegisterSomeService(BundleContext context, const std::shared_ptr<SomeInterface>& service)
    {
      context.RegisterService<SomeInterface>(service);
    }

The OSGi service model additionally allows to annotate services with
properties and using these properties during service look-ups. It also
allows to track the life-cycle of service objects. Please see the
`Documentation <http://docs.cppmicroservices.org>`_
for more examples and tutorials and the API reference. There is also a
blog post about `OSGi Lite for C++ <http://blog.cppmicroservices.org/2012/04/15/osgi-lite-for-c++>`_.

Git Branch Conventions
----------------------

The Git repository contains two eternal branches,
`master <https://github.com/CppMicroServices/CppMicroServices/tree/master/>`_
and
`development <https://github.com/CppMicroServices/CppMicroServices/tree/development/>`_.
The master branch contains production quality code and its HEAD points
to the latest released version. The development branch is the default
branch and contains the current state of development. Pull requests by
default target the development branch. See the :ref:`CONTRIBUTING <contributing>`
file for details about the contribution process.


.. _COPYRIGHT file: https://github.com/CppMicroServices/CppMicroServices/blob/development/COPYRIGHT
.. _GitHub Project: https://github.com/CppMicroServices/CppMicroServices
.. _Apache License v2.0: http://www.apache.org/licenses/LICENSE-2.0

.. |BuildAndTestNix| image:: https://github.com/CppMicroServices/CppMicroServices/actions/workflows/build_and_test_nix.yml/badge.svg?branch=development&event=push
   :target: https://github.com/CppMicroServices/CppMicroServices/actions/workflows/build_and_test_nix.yml
.. |BuildAndTestNix(master)| image:: https://github.com/CppMicroServices/CppMicroServices/actions/workflows/build_and_test_nix.yml/badge.svg?branch=master&event=push
   :target: https://github.com/CppMicroServices/CppMicroServices/actions/workflows/build_and_test_nix.yml
.. |BuildAndTestWindows| image:: https://github.com/CppMicroServices/CppMicroServices/actions/workflows/build_and_test_windows.yml/badge.svg?branch=development&event=push
   :target: https://github.com/CppMicroServices/CppMicroServices/actions/workflows/build_and_test_windows.yml
.. |BuildAndTestWindows(master)| image:: https://github.com/CppMicroServices/CppMicroServices/actions/workflows/build_and_test_windows.yml/badge.svg?branch=master&event=push
   :target: https://github.com/CppMicroServices/CppMicroServices/actions/workflows/build_and_test_windows.yml   
.. |Coverity Scan Build Status| image:: https://img.shields.io/coverity/scan/1329.svg?style=flat-square
   :target: https://scan.coverity.com/projects/1329
.. |RTD Build Status (stable)| image:: https://readthedocs.org/projects/cppmicroservices/badge/?version=stable&style=flat-square
   :target: http://docs.cppmicroservices.org/en/stable/?badge=stable
   :alt: Documentation Status (stable)
.. |RTD Build Status (development)| image:: https://readthedocs.org/projects/cppmicroservices/badge/?version=latest&style=flat-square
   :target: http://docs.cppmicroservices.org/en/latest/?badge=development
   :alt: Documentation Status (development)
.. |Code Coverage Status| image:: https://img.shields.io/codecov/c/github/CppMicroServices/CppMicroServices/master.svg?style=flat-square
   :target: https://codecov.io/gh/cppmicroservices/CppMicroServices/branch/master
.. |Code Coverage Status (development)| image:: https://img.shields.io/codecov/c/github/CppMicroServices/CppMicroServices/development.svg?style=flat-square
   :target: https://codecov.io/gh/cppmicroservices/CppMicroServices/branch/development
.. |Performance Status| image:: https://github.com/CppMicroServices/CppMicroServices/actions/workflows/performance_windows.yml/badge.svg
   :target: https://cppmicroservices.org/dev/bench/

Git Hooks General Information
-----------------------------

The CppMicroServices repository defines its git hooks in the `.githooks` directory. This directory is
set as the directory for git hooks via executing `git config core.hooksPath <path>` in our `CMakeLists.txt` file.

Git Hooks Failure Help
----------------------

If the clang-format pre-commit hook fails because `clang-format` is not installed, please install it and
put it on the path. Similarly, if `git-clang-format` is not installed, do the same. `git-clang-format` comes
with the LLVM distribution of `clang-format`.

If this is not feasible for you, you can specify `--no-verify` when committing your changes. This is heavily discouraged
and you must provide a justification as to why you are unable to format your commit.

We reserve the right to reject any pull requests that are not properly formatted and do not have a
valid justification specified.
