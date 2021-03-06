// Copyright (c) 2015-2016 The Khronos Group Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

#include "source/spirv_target_env.h"
#include "source/spirv_validator_options.h"
#include "spirv-tools/libspirv.hpp"
#include "tools/io.h"

void print_usage(char* argv0) {
  printf(
      R"(%s - Validate a SPIR-V binary file.

USAGE: %s [options] [<filename>]

The SPIR-V binary is read from <filename>. If no file is specified,
or if the filename is "-", then the binary is read from standard input.

NOTE: The validator is a work in progress.

Options:
  -h, --help                       Print this help.
  --max-struct-members             <maximum number of structure members allowed>
  --max-struct-depth               <maximum allowed nesting depth of structures>
  --max-local-variables            <maximum number of local variables allowed>
  --max-global-variables           <maximum number of global variables allowed>
  --max-switch-branches            <maximum number of branches allowed in switch statements>
  --max-function-args              <maximum number arguments allowed per function>
  --max-control-flow-nesting-depth <maximum Control Flow nesting depth allowed>
  --max-access-chain-indexes       <maximum number of indexes allowed to use for Access Chain instructions>
  --relax-logcial-pointer          Allow allocating an object of a pointer type and returning
                                   a pointer value from a function in logical addressing mode
  --relax-struct-store             Allow store from one struct type to a
                                   different type with compatible layout and
                                   members.
  --version                        Display validator version information.
  --target-env                     {vulkan1.0|spv1.0|spv1.1|spv1.2}
                                   Use Vulkan1.0/SPIR-V1.0/SPIR-V1.1/SPIR-V1.2 validation rules.
)",
      argv0, argv0);
}

int main(int argc, char** argv) {
  const char* inFile = nullptr;
  spv_target_env target_env = SPV_ENV_UNIVERSAL_1_2;
  spvtools::ValidatorOptions options;
  bool continue_processing = true;
  int return_code = 0;

  for (int argi = 1; continue_processing && argi < argc; ++argi) {
    const char* cur_arg = argv[argi];
    if ('-' == cur_arg[0]) {
      if (0 == strncmp(cur_arg, "--max-", 6)) {
        if (argi + 1 < argc) {
          spv_validator_limit limit_type;
          if (spvParseUniversalLimitsOptions(cur_arg, &limit_type)) {
            uint32_t limit = 0;
            if (sscanf(argv[++argi], "%d", &limit)) {
              options.SetUniversalLimit(limit_type, limit);
            } else {
              fprintf(stderr, "error: missing argument to %s\n", cur_arg);
              continue_processing = false;
              return_code = 1;
            }
          } else {
            fprintf(stderr, "error: unrecognized option: %s\n", cur_arg);
            continue_processing = false;
            return_code = 1;
          }
        } else {
          fprintf(stderr, "error: Missing argument to %s\n", cur_arg);
          continue_processing = false;
          return_code = 1;
        }
      } else if (0 == strcmp(cur_arg, "--version")) {
        printf("%s\n", spvSoftwareVersionDetailsString());
        // TODO(dneto): Add OpenCL 2.2 at least.
        printf("Targets:\n  %s\n  %s\n  %s\n",
               spvTargetEnvDescription(SPV_ENV_UNIVERSAL_1_1),
               spvTargetEnvDescription(SPV_ENV_VULKAN_1_0),
               spvTargetEnvDescription(SPV_ENV_UNIVERSAL_1_2));
        continue_processing = false;
        return_code = 0;
      } else if (0 == strcmp(cur_arg, "--help") || 0 == strcmp(cur_arg, "-h")) {
        print_usage(argv[0]);
        continue_processing = false;
        return_code = 0;
      } else if (0 == strcmp(cur_arg, "--target-env")) {
        if (argi + 1 < argc) {
          const auto env_str = argv[++argi];
          if (!spvParseTargetEnv(env_str, &target_env)) {
            fprintf(stderr, "error: Unrecognized target env: %s\n", env_str);
            continue_processing = false;
            return_code = 1;
          }
        } else {
          fprintf(stderr, "error: Missing argument to --target-env\n");
          continue_processing = false;
          return_code = 1;
        }
      } else if (0 == strcmp(cur_arg, "--relax-logical-pointer")) {
        options.SetRelaxLogicalPointer(true);
      } else if (0 == strcmp(cur_arg, "--relax-struct-store")) {
        options.SetRelaxStructStore(true);
      } else if (0 == cur_arg[1]) {
        // Setting a filename of "-" to indicate stdin.
        if (!inFile) {
          inFile = cur_arg;
        } else {
          fprintf(stderr, "error: More than one input file specified\n");
          continue_processing = false;
          return_code = 1;
        }
      } else {
        print_usage(argv[0]);
        continue_processing = false;
        return_code = 1;
      }
    } else {
      if (!inFile) {
        inFile = cur_arg;
      } else {
        fprintf(stderr, "error: More than one input file specified\n");
        continue_processing = false;
        return_code = 1;
      }
    }
  }

  // Exit if command line parsing was not successful.
  if (!continue_processing) {
    return return_code;
  }

  std::vector<uint32_t> contents;
  if (!ReadFile<uint32_t>(inFile, "rb", &contents)) return 1;

  spvtools::SpirvTools tools(target_env);
  tools.SetMessageConsumer([](spv_message_level_t level, const char*,
                              const spv_position_t& position,
                              const char* message) {
    switch (level) {
      case SPV_MSG_FATAL:
      case SPV_MSG_INTERNAL_ERROR:
      case SPV_MSG_ERROR:
        std::cerr << "error: " << position.index << ": " << message
                  << std::endl;
        break;
      case SPV_MSG_WARNING:
        std::cout << "warning: " << position.index << ": " << message
                  << std::endl;
        break;
      case SPV_MSG_INFO:
        std::cout << "info: " << position.index << ": " << message << std::endl;
        break;
      default:
        break;
    }
  });

  bool succeed = tools.Validate(contents.data(), contents.size(), options);

  return !succeed;
}
