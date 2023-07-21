// Copyright (c) 2023- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include "Core/MemMap.h"
#include "Core/MIPS/RiscV/RiscVJit.h"
#include "Core/MIPS/RiscV/RiscVRegCache.h"

// This file contains compilation for basic PC/downcount accounting, debug funcs, etc.
//
// All functions should have CONDITIONAL_DISABLE, so we can narrow things down to a file quickly.
// Currently known non working ones should have DISABLE.  No flags because that's in IR already.

// #define CONDITIONAL_DISABLE { CompIR_Generic(inst); return; }
#define CONDITIONAL_DISABLE {}
#define DISABLE { CompIR_Generic(inst); return; }
#define INVALIDOP { _assert_msg_(false, "Invalid IR inst %d", (int)inst.op); CompIR_Generic(inst); return; }

namespace MIPSComp {

using namespace RiscVGen;
using namespace RiscVJitConstants;

void RiscVJit::CompIR_Basic(IRInst inst) {
	CONDITIONAL_DISABLE;

	switch (inst.op) {
	case IROp::SetConst:
		gpr.SetImm(inst.dest, inst.constant);
		break;

	case IROp::Downcount:
		if (inst.constant <= 2048) {
			ADDI(DOWNCOUNTREG, DOWNCOUNTREG, -(s32)inst.constant);
		} else {
			LI(SCRATCH1, inst.constant, SCRATCH2);
			SUB(DOWNCOUNTREG, DOWNCOUNTREG, SCRATCH1);
		}
		break;

	case IROp::SetPC:
		gpr.MapIn(inst.src1);
		MovToPC(gpr.R(inst.src1));
		break;

	case IROp::SetPCConst:
		LI(SCRATCH1, inst.constant, SCRATCH2);
		MovToPC(SCRATCH1);
		break;

	default:
		INVALIDOP;
		break;
	}
}

} // namespace MIPSComp
