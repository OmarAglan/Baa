/**
 * @file driver_internal.h
 * @brief ????? ?????? ????? ?????? ?????? (Driver).
 */

#ifndef BAA_DRIVER_INTERNAL_H
#define BAA_DRIVER_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#include "driver.h"
#include "driver_cli.h"
#include "driver_pipeline.h"
#include "driver_build.h"
#include "driver_time.h"
#include "driver_toolchain.h"
#include "process.h"
#include "../support/version.h"
#include "../support/read_file.h"
#include "../support/diagnostics.h"
#include "../support/updater.h"
#include "../frontend/lexer.h"
#include "../frontend/parser.h"
#include "../frontend/analysis.h"
#include "../backend/emit.h"
#include "../backend/isel.h"
#include "../backend/regalloc.h"
#include "../middleend/ir_arena.h"
#include "../middleend/ir_lower.h"
#include "../middleend/ir_optimizer.h"
#include "../middleend/ir_outssa.h"
#include "../middleend/ir_unroll.h"
#include "../middleend/ir_verify_ir.h"
#include "../middleend/ir_verify_ssa.h"

#endif
