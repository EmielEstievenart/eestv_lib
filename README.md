# eestv_lib

The reusable library part of my personal `eestv` project.

This directory is intended to stay lightweight so it works well as a submodule
and does not add much size to the superproject. It is not a standalone project,
though: the parent project is expected to provide the build setup, tests, and
third-party dependencies.

Development happens in the main `eestv` repository:
https://github.com/EmielEstievenart/eestv

## Dependencies

`eestv_lib` currently depends on these Boost components:

* `Boost::asio`
* `Boost::system`
* `Boost::program_options`

## How Boost Is Provided

This submodule does not fetch or locate Boost on its own. It expects the parent
project to provide Boost as part of the build setup.

In the main `eestv` project, this is done by:

* setting `BOOST_ROOT` to the root of a Boost source tree
* enabling Boost's CMake support
* adding Boost with `add_subdirectory(${BOOST_ROOT} ...)`

So if you reuse `eestv_lib` elsewhere, the consuming project must make the
required Boost targets available before linking `eestv_lib`.
