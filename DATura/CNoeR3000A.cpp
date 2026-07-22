//Displaying: CNoeR3000A.cpp

//===============================================================================
// Noesis R3000A CPU core
// (c) whenever Rich Whitehouse
// an easily-debuggable and pliable R3000A core.
// has been live-tested inside a PSX emulator with SotN, but not too extensively.
//===============================================================================

#include "CNoeR3000A.h"

#define NOER3000A_DISASM_VALUES() 0 //useful for debugging - shows register values as ints when disassembling from a live context

struct SR3000ADefaultOpHandler
{
	typedef void (*TDefaultOpHandlerFunction)(CNoeR3000A *pCpu, const uint32_t opVal, const SR3000ADefaultOpHandler *pHandler);
	typedef void (*TDefaultOpStringFormatFunction)(char *pStringOut, CNoeR3000A *pCpu, const uint32_t opVal, const uint32_t opAddr, const SR3000ADefaultOpHandler *pHandler);

	explicit SR3000ADefaultOpHandler(const char *pOpName, TDefaultOpHandlerFunction pOpHandler, TDefaultOpStringFormatFunction pOpStringFormat)
		: mpOpName(pOpName)
		, mpOpHandler(pOpHandler)
		, mpOpStringFormat(pOpStringFormat)
	{
		NoeAssert(strlen(pOpName) <= CNoeR3000A::skMaximumOpNameLength);
	}

	const char *mpOpName;
	TDefaultOpHandlerFunction mpOpHandler;
	TDefaultOpStringFormatFunction mpOpStringFormat;
};

namespace
{
	const uint64_t skDefaultCyclesPerOp = 1;

	const uint32_t skDefaultDelayBranchCycles = 0;
	const uint32_t skDefaultDelayLoadCycles = 0;

	const char *skGPRNameForIndex[CNoeR3000A::kGPR_Count] =
	{
		"r0", //kGPR_Zero,
		"at", //kGPR_AT,
		"v0", //kGPR_V0,
		"v1", //kGPR_V1,
		"a0", //kGPR_A0,
		"a1", //kGPR_A1,
		"a2", //kGPR_A2,
		"a3", //kGPR_A3,
		"t0", //kGPR_T0,
		"t1", //kGPR_T1,
		"t2", //kGPR_T2,
		"t3", //kGPR_T3,
		"t4", //kGPR_T4,
		"t5", //kGPR_T5,
		"t6", //kGPR_T6,
		"t7", //kGPR_T7,
		"s0", //kGPR_S0,
		"s1", //kGPR_S1,
		"s2", //kGPR_S2,
		"s3", //kGPR_S3,
		"s4", //kGPR_S4,
		"s5", //kGPR_S5,
		"s6", //kGPR_S6,
		"s7", //kGPR_S7,
		"t8", //kGPR_T8,
		"t9", //kGPR_T9,
		"k0", //kGPR_K0,
		"k1", //kGPR_K1,
		"gp", //kGPR_GP,
		"sp", //kGPR_SP,
		"fp", //kGPR_FP,
		"ra" //kGPR_RA,  
	};

	const char *skIRNameForIndex[CNoeR3000A::kIR_Count] =
	{
		"lo", //kIR_Lo,
		"hi", //kIR_Hi,
		"pc", //kIR_PC,
		"op", //kIR_Op,
	};


	template<typename DataType>
	DataType default_read_memory(CNoeR3000A *pCpu, const uint32_t addr, const CNoeR3000A::EAccessType accessType)
	{
		return 0;
	}

	template<typename DataType>
	void default_write_memory(CNoeR3000A *pCpu, const uint32_t addr, const DataType memVal, const CNoeR3000A::EAccessType accessType)
	{
	}

	void default_write_cop0_reg(CNoeR3000A *pCpu, const CNoeR3000A::EC0R cop0Reg, const uint32_t regVal)
	{
		pCpu->C0R()[cop0Reg] = regVal;
	}

#if NOER3000A_DEBUGGER()
	CNoeDebugger::EDebuggerReadWriteResult debugger_read_data(CNoeDebugger *pDebugger, void *pDest, const uint64_t readAddr, const uint32_t readSize)
	{
		CNoeR3000A *pCpu = static_cast<CNoeR3000A *>(pDebugger->GetUserData());
		switch (readSize)
		{
		case 1:
			*static_cast<uint8_t *>(pDest) = pCpu->ReadMemory8(static_cast<uint32_t>(readAddr), CNoeR3000A::kAT_External);
			break;
		case 2:
			*static_cast<uint16_t *>(pDest) = pCpu->ReadMemory16(static_cast<uint32_t>(readAddr), CNoeR3000A::kAT_External);
			break;
		case 4:
			*static_cast<uint32_t *>(pDest) = pCpu->ReadMemory32(static_cast<uint32_t>(readAddr), CNoeR3000A::kAT_External);
			break;
		default:
			for (uint32_t readAmount = 0; readAmount < readSize; ++readAmount)
			{
				static_cast<uint8_t *>(pDest)[readAmount] = pCpu->ReadMemory8(static_cast<uint32_t>(readAddr + readAmount), CNoeR3000A::kAT_External);
			}
			break;
		}
		return CNoeDebugger::kDRWR_Success;
	}

	CNoeDebugger::EDebuggerReadWriteResult debugger_write_data(CNoeDebugger *pDebugger, const void *pSrc, const uint64_t writeAddr, const uint32_t writeSize)
	{
		CNoeR3000A *pCpu = static_cast<CNoeR3000A *>(pDebugger->GetUserData());
		switch (writeSize)
		{
		case 1:
			pCpu->WriteMemory8(static_cast<uint32_t>(writeAddr), *static_cast<const uint8_t *>(pSrc), CNoeR3000A::kAT_External);
			break;
		case 2:
			pCpu->WriteMemory16(static_cast<uint32_t>(writeAddr), *static_cast<const uint16_t *>(pSrc), CNoeR3000A::kAT_External);
			break;
		case 4:
			pCpu->WriteMemory32(static_cast<uint32_t>(writeAddr), *static_cast<const uint32_t *>(pSrc), CNoeR3000A::kAT_External);
			break;
		default:
			for (uint32_t writeAmount = 0; writeAmount < writeSize; ++writeAmount)
			{
				pCpu->WriteMemory8(static_cast<uint32_t>(writeAddr + writeAmount), static_cast<const uint8_t *>(pSrc)[writeAmount], CNoeR3000A::kAT_External);
			}
			break;
		}
		return CNoeDebugger::kDRWR_Success;
	}

	uint32_t *debugger_reg_pointer(CNoeDebugger *pDebugger, const uint32_t regIndex)
	{
		CNoeR3000A *pCpu = static_cast<CNoeR3000A *>(pDebugger->GetUserData());
		if (regIndex < CNoeR3000A::kGPR_Count)
		{
			return &pCpu->GPR()[regIndex];
		}
		else if (regIndex < (CNoeR3000A::kGPR_Count + CNoeR3000A::kIR_Count))
		{
			return &pCpu->IR()[regIndex - CNoeR3000A::kGPR_Count];
		}
		else if (regIndex < (CNoeR3000A::kGPR_Count + CNoeR3000A::kIR_Count + CNoeR3000A::kC0R_Count))
		{
			return &pCpu->C0R()[regIndex - (CNoeR3000A::kGPR_Count + CNoeR3000A::kIR_Count)];
		}
		return NULL;
	}

	CNoeDebugger::EDebuggerReadWriteResult debugger_read_reg(CNoeDebugger *pDebugger, void *pDest, const uint32_t regIndex)
	{
		if (uint32_t *pReg = debugger_reg_pointer(pDebugger, regIndex))
		{
			memcpy(pDest, pReg, sizeof(uint32_t));
			return CNoeDebugger::kDRWR_Success;
		}
		return CNoeDebugger::kDRWR_Failure;
	}

	CNoeDebugger::EDebuggerReadWriteResult debugger_write_reg(CNoeDebugger *pDebugger, const void *pSrc, const uint32_t regIndex)
	{
		if (uint32_t *pReg = debugger_reg_pointer(pDebugger, regIndex))
		{
			memcpy(pReg, pSrc, sizeof(uint32_t));
			return CNoeDebugger::kDRWR_Success;
		}
		return CNoeDebugger::kDRWR_Failure;
	}

	void debugger_disassemble(CNoeDebugger *pDebugger, CNoeMemWriteBuffer *pDisasmOut, const uint64_t addr, const uint64_t refAddr, const uint32_t instructionCount)
	{
		CNoeR3000A *pCpu = static_cast<CNoeR3000A *>(pDebugger->GetUserData());
		char disasmString[CNoeR3000A::skMaximumOpStringLength];
		for (uint32_t instructionIndex = 0; instructionIndex < instructionCount; ++instructionIndex)
		{
			const uint32_t opAddr = static_cast<uint32_t>(addr) + instructionIndex * CNoeR3000A::skInstructionSize;
			const uint32_t opVal = pCpu->ReadInstruction(opAddr, CNoeR3000A::kAT_External);
			pCpu->FormatOpString(disasmString, opVal, opAddr);
			NoeAssert(CNoeR3000A::skInstructionSize == sizeof(opVal));
			pDisasmOut->Write(NNoeDebugProtocol::SDisasmEntry(opAddr, CNoeR3000A::skInstructionSize));
			pDisasmOut->Write(opVal);
			pDisasmOut->WriteString("%s", disasmString);
		}
	}
#endif

	#define OP_OB(opName) SR3000ADefaultOpHandler(#opName, CNoeR3000AOps::opName, CNoeR3000AOps::opName##_format)
	#define OP_IMPLEMENT(opName) static void opName(CNoeR3000A *pCpu, const uint32_t opVal, const SR3000ADefaultOpHandler *pHandlerObject)
	#define OP_IMPLEMENT_FMT(opName) static void opName##_format(char *pStringOut, CNoeR3000A *pCpu, const uint32_t opVal, const uint32_t opAddr, const SR3000ADefaultOpHandler *pHandlerObject)

	uint32_t &reg_sa(CNoeR3000A *pCpu, const uint32_t opVal)
	{
		const uint32_t regIndex = CNoeR3000A::GetInstructionSA(opVal);
		return pCpu->GPR()[regIndex];
	}
	uint32_t &reg_rd(CNoeR3000A *pCpu, const uint32_t opVal)
	{
		const uint32_t regIndex = CNoeR3000A::GetInstructionRD(opVal);
		return pCpu->GPR()[regIndex];
	}
	uint32_t &reg_rt(CNoeR3000A *pCpu, const uint32_t opVal)
	{
		const uint32_t regIndex = CNoeR3000A::GetInstructionRT(opVal);
		return pCpu->GPR()[regIndex];
	}
	uint32_t &reg_rs(CNoeR3000A *pCpu, const uint32_t opVal)
	{
		const uint32_t regIndex = CNoeR3000A::GetInstructionRS(opVal);
		return pCpu->GPR()[regIndex];
	}

	bool sa_is_r0(const uint32_t opVal)
	{
		return CNoeR3000A::GetInstructionSA(opVal) == CNoeR3000A::kGPR_Zero;
	}
	bool rd_is_r0(const uint32_t opVal)
	{
		return CNoeR3000A::GetInstructionRD(opVal) == CNoeR3000A::kGPR_Zero;
	}
	bool rt_is_r0(const uint32_t opVal)
	{
		return CNoeR3000A::GetInstructionRT(opVal) == CNoeR3000A::kGPR_Zero;
	}
	bool rs_is_r0(const uint32_t opVal)
	{
		return CNoeR3000A::GetInstructionRS(opVal) == CNoeR3000A::kGPR_Zero;
	}

	uint32_t op_imm_16(const uint32_t opVal)
	{
		return (opVal & 0xFFFF);
	}

	int16_t op_imm_s16(const uint32_t opVal)
	{
		return int16_t(opVal & 0xFFFF);
	}

	const int skCircularTempStringBufferSize = CNoeR3000A::skMaximumOpStringLength * 8;
	char sCircularTempStringBuffer[skCircularTempStringBufferSize];
	int sCircularTempStringBufferOffset = 0;
	char *get_temp_string_buffer(const int bufferSize)
	{
		NoeAssert(bufferSize < skCircularTempStringBufferSize);
		if ((sCircularTempStringBufferOffset + bufferSize) > skCircularTempStringBufferSize)
		{
			//wrap around
			sCircularTempStringBufferOffset = 0;
		}

		char *pBuffer = &sCircularTempStringBuffer[sCircularTempStringBufferOffset];
		sCircularTempStringBufferOffset += bufferSize;

		return pBuffer;
	}

	const char *string_for_op(const SR3000ADefaultOpHandler *pHandlerObject)
	{
		const int opNameLength = strlen(pHandlerObject->mpOpName);
		const int paddedNameLength = CNoeR3000A::skMaximumOpNameLength + 4;
		char *pOpString = get_temp_string_buffer(paddedNameLength + 1);

		memcpy(pOpString, pHandlerObject->mpOpName, opNameLength);
		memset(pOpString + opNameLength, ' ', paddedNameLength - opNameLength);
		pOpString[paddedNameLength] = 0;

		return pOpString;
	}

#if NOER3000A_DISASM_VALUES()
	const char *reg_string_internal(CNoeR3000A *pCpu, const uint32_t regIndex)
	{
		char *pRegString = get_temp_string_buffer(32);
		NoeSPrintF(pRegString, 32, "%s(%i)", CNoeR3000A::GetGPRName(CNoeR3000A::EGPR(regIndex)), pCpu->GPR()[regIndex]);
		return pRegString;
	}
	const char *reg_string_sa_internal(CNoeR3000A *pCpu, const uint32_t opVal)
	{
		return reg_string_internal(pCpu, CNoeR3000A::GetInstructionSA(opVal));
	}
	const char *reg_string_rd_internal(CNoeR3000A *pCpu, const uint32_t opVal)
	{
		return reg_string_internal(pCpu, CNoeR3000A::GetInstructionRD(opVal));
	}
	const char *reg_string_rt_internal(CNoeR3000A *pCpu, const uint32_t opVal)
	{
		return reg_string_internal(pCpu, CNoeR3000A::GetInstructionRT(opVal));
	}
	const char *reg_string_rs_internal(CNoeR3000A *pCpu, const uint32_t opVal)
	{
		return reg_string_internal(pCpu, CNoeR3000A::GetInstructionRS(opVal));
	}
	#define reg_string_sa(opVal) reg_string_sa_internal(pCpu, opVal)
	#define reg_string_rd(opVal) reg_string_rd_internal(pCpu, opVal)
	#define reg_string_rt(opVal) reg_string_rt_internal(pCpu, opVal)
	#define reg_string_rs(opVal) reg_string_rs_internal(pCpu, opVal)
#else
	const char *reg_string_sa(const uint32_t opVal)
	{
		return CNoeR3000A::GetGPRName(CNoeR3000A::EGPR(CNoeR3000A::GetInstructionSA(opVal)));
	}
	const char *reg_string_rd(const uint32_t opVal)
	{
		return CNoeR3000A::GetGPRName(CNoeR3000A::EGPR(CNoeR3000A::GetInstructionRD(opVal)));
	}
	const char *reg_string_rt(const uint32_t opVal)
	{
		return CNoeR3000A::GetGPRName(CNoeR3000A::EGPR(CNoeR3000A::GetInstructionRT(opVal)));
	}
	const char *reg_string_rs(const uint32_t opVal)
	{
		return CNoeR3000A::GetGPRName(CNoeR3000A::EGPR(CNoeR3000A::GetInstructionRS(opVal)));
	}
#endif
	const char *cop0_reg_string(const uint32_t regIndex)
	{
		NoeAssert(regIndex < CNoeR3000A::kC0R_Count);
		char *pRegString = get_temp_string_buffer(4);
		NoeSPrintF(pRegString, 4, "c%i", regIndex);
		return pRegString;
	}

	void jump_string_format(char *pStringOut, CNoeR3000A *pCpu, const uint32_t opVal, const uint32_t opAddr, const SR3000ADefaultOpHandler *pHandlerObject)
	{
		const uint32_t jumpAddr = CNoeR3000A::GetInstructionJumpAddress(opVal) +
		                          (opAddr & CNoeR3000A::skInstructionJumpAddressPCMask);
		NoeSPrintF(pStringOut, CNoeR3000A::skMaximumOpStringLength, "%s$%0x",
			string_for_op(pHandlerObject), jumpAddr);
	}

	void conditional_branch_string_format(char *pStringOut, CNoeR3000A *pCpu, const uint32_t opVal, const uint32_t opAddr, const SR3000ADefaultOpHandler *pHandlerObject)
	{
		NoeSPrintF(pStringOut, CNoeR3000A::skMaximumOpStringLength, "%s%s, $%0x",
			string_for_op(pHandlerObject), reg_string_rs(opVal), opAddr + (op_imm_16(opVal) << 2));
	}

	void conditional_branch_reg_string_format(char *pStringOut, CNoeR3000A *pCpu, const uint32_t opVal, const uint32_t opAddr, const SR3000ADefaultOpHandler *pHandlerObject)
	{
		NoeSPrintF(pStringOut, CNoeR3000A::skMaximumOpStringLength, "%s%s, %s, $%0x",
			string_for_op(pHandlerObject), reg_string_rs(opVal), reg_string_rt(opVal), opAddr + (op_imm_16(opVal) << 2));
	}

	void load_store_string_format(char *pStringOut, CNoeR3000A *pCpu, const uint32_t opVal, const SR3000ADefaultOpHandler *pHandlerObject)
	{
		NoeSPrintF(pStringOut, CNoeR3000A::skMaximumOpStringLength, "%s%s, %i(%s)",
			string_for_op(pHandlerObject), reg_string_rt(opVal), op_imm_s16(opVal), reg_string_rs(opVal));
	}

	void op_reg_st_string_format(char *pStringOut, CNoeR3000A *pCpu, const uint32_t opVal, const SR3000ADefaultOpHandler *pHandlerObject)
	{
		NoeSPrintF(pStringOut, CNoeR3000A::skMaximumOpStringLength, "%s%s, %s",
			string_for_op(pHandlerObject), reg_string_rs(opVal), reg_string_rt(opVal));
	}

	void op_reg_ts_is16_string_format(char *pStringOut, CNoeR3000A *pCpu, const uint32_t opVal, const SR3000ADefaultOpHandler *pHandlerObject)
	{
		NoeSPrintF(pStringOut, CNoeR3000A::skMaximumOpStringLength, "%s%s, %s, %i",
			string_for_op(pHandlerObject), reg_string_rt(opVal), reg_string_rs(opVal), int32_t(op_imm_s16(opVal)));
	}

	void op_reg_ts_iu16_string_format(char *pStringOut, CNoeR3000A *pCpu, const uint32_t opVal, const SR3000ADefaultOpHandler *pHandlerObject)
	{
		NoeSPrintF(pStringOut, CNoeR3000A::skMaximumOpStringLength, "%s%s, %s, %i",
			string_for_op(pHandlerObject), reg_string_rt(opVal), reg_string_rs(opVal), op_imm_16(opVal));
	}

	void op_reg_dt_sa_string_format(char *pStringOut, CNoeR3000A *pCpu, const uint32_t opVal, const SR3000ADefaultOpHandler *pHandlerObject)
	{
		NoeSPrintF(pStringOut, CNoeR3000A::skMaximumOpStringLength, "%s%s, %s, %i",
			string_for_op(pHandlerObject), reg_string_rd(opVal), reg_string_rt(opVal), CNoeR3000A::GetInstructionSA(opVal));
	}

	void op_reg_d_string_format(char *pStringOut, CNoeR3000A *pCpu, const uint32_t opVal, const SR3000ADefaultOpHandler *pHandlerObject)
	{
		NoeSPrintF(pStringOut, CNoeR3000A::skMaximumOpStringLength, "%s%s",
			string_for_op(pHandlerObject), reg_string_rd(opVal));
	}

	void op_reg_s_string_format(char *pStringOut, CNoeR3000A *pCpu, const uint32_t opVal, const SR3000ADefaultOpHandler *pHandlerObject)
	{
		NoeSPrintF(pStringOut, CNoeR3000A::skMaximumOpStringLength, "%s%s",
			string_for_op(pHandlerObject), reg_string_rs(opVal));
	}

	void op_reg_dts_string_format(char *pStringOut, CNoeR3000A *pCpu, const uint32_t opVal, const SR3000ADefaultOpHandler *pHandlerObject)
	{
		NoeSPrintF(pStringOut, CNoeR3000A::skMaximumOpStringLength, "%s%s, %s, %s",
			string_for_op(pHandlerObject), reg_string_rd(opVal), reg_string_rt(opVal), reg_string_rs(opVal));
	}

	void op_reg_dst_string_format(char *pStringOut, CNoeR3000A *pCpu, const uint32_t opVal, const SR3000ADefaultOpHandler *pHandlerObject)
	{
		NoeSPrintF(pStringOut, CNoeR3000A::skMaximumOpStringLength, "%s%s, %s, %s",
			string_for_op(pHandlerObject), reg_string_rd(opVal), reg_string_rs(opVal), reg_string_rt(opVal));
	}
}

class CNoeR3000AOps
{
public:
	//invalid op (currently, just ignore)
	OP_IMPLEMENT(invalid)
	{
	}
	OP_IMPLEMENT_FMT(invalid)
	{
		NoeSPrintF(pStringOut, CNoeR3000A::skMaximumOpStringLength, "<invalid op>");
	}

	//nop
	OP_IMPLEMENT(nop)
	{
	}
	OP_IMPLEMENT_FMT(nop)
	{
		NoeSPrintF(pStringOut, CNoeR3000A::skMaximumOpStringLength, "nop");
	}

	//redirect to table2
	OP_IMPLEMENT(table2)
	{
		const SR3000ADefaultOpHandler &opHandler = CNoeR3000A::mspTable2OpHandlers[CNoeR3000A::GetInstructionOp2(opVal)];
		opHandler.mpOpHandler(pCpu, opVal, &opHandler);
	}
	OP_IMPLEMENT_FMT(table2)
	{
		const SR3000ADefaultOpHandler &opHandler = CNoeR3000A::mspTable2OpHandlers[CNoeR3000A::GetInstructionOp2(opVal)];
		opHandler.mpOpStringFormat(pStringOut, pCpu, opVal, opAddr, &opHandler);
	}

	//redirect to conditional branch table
	OP_IMPLEMENT(cbranch)
	{
		const SR3000ADefaultOpHandler &opHandler = CNoeR3000A::mspCBranchOpHandlers[CNoeR3000A::GetInstructionRT(opVal)];
		opHandler.mpOpHandler(pCpu, opVal, &opHandler);
	}
	OP_IMPLEMENT_FMT(cbranch)
	{
		const SR3000ADefaultOpHandler &opHandler = CNoeR3000A::mspCBranchOpHandlers[CNoeR3000A::GetInstructionRT(opVal)];
		opHandler.mpOpStringFormat(pStringOut, pCpu, opVal, opAddr, &opHandler);
	}

	//jump
	OP_IMPLEMENT(j)
	{
		const uint32_t jumpAddr = CNoeR3000A::GetInstructionJumpAddress(opVal) +
		                          (pCpu->mpIR[CNoeR3000A::kIR_PC] & CNoeR3000A::skInstructionJumpAddressPCMask);
		pCpu->TakeBranch(jumpAddr);
	}
	OP_IMPLEMENT_FMT(j)
	{
		jump_string_format(pStringOut, pCpu, opVal, opAddr, pHandlerObject);
	}

	//jump register
	OP_IMPLEMENT(jr)
	{
		pCpu->TakeBranch(reg_rs(pCpu, opVal));
	}
	OP_IMPLEMENT_FMT(jr)
	{
		op_reg_s_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//jump and link register
	OP_IMPLEMENT(jalr)
	{
		if (!rd_is_r0(opVal))
		{
			reg_rd(pCpu, opVal) = pCpu->mpIR[CNoeR3000A::kIR_PC] + CNoeR3000A::skInstructionSize;
		}
		pCpu->TakeBranch(reg_rs(pCpu, opVal));
	}
	OP_IMPLEMENT_FMT(jalr)
	{
		NoeSPrintF(pStringOut, CNoeR3000A::skMaximumOpStringLength, "%s%s, %s",
			string_for_op(pHandlerObject), reg_string_rd(opVal), reg_string_rs(opVal));
	}

	//jump and link
	OP_IMPLEMENT(jal)
	{
		const uint32_t jumpAddr = CNoeR3000A::GetInstructionJumpAddress(opVal) +
		                          (pCpu->mpIR[CNoeR3000A::kIR_PC] & CNoeR3000A::skInstructionJumpAddressPCMask);
		SetReturnAddress(pCpu);
		pCpu->TakeBranch(jumpAddr);
	}
	OP_IMPLEMENT_FMT(jal)
	{
		jump_string_format(pStringOut, pCpu, opVal, opAddr, pHandlerObject);
	}

	//add immediate
	OP_IMPLEMENT(addi)
	{
		if (!rt_is_r0(opVal))
		{
			reg_rt(pCpu, opVal) = reg_rs(pCpu, opVal) + op_imm_s16(opVal);
		}
	}
	OP_IMPLEMENT_FMT(addi)
	{
		op_reg_ts_is16_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//add immediate unsigned
	OP_IMPLEMENT(addiu)
	{
		if (!rt_is_r0(opVal))
		{
			reg_rt(pCpu, opVal) = reg_rs(pCpu, opVal) + op_imm_s16(opVal);
		}
	}
	OP_IMPLEMENT_FMT(addiu)
	{
		op_reg_ts_is16_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//set on less than immediate
	OP_IMPLEMENT(slti)
	{
		if (!rt_is_r0(opVal))
		{
			reg_rt(pCpu, opVal) = (int32_t(reg_rs(pCpu, opVal)) < op_imm_s16(opVal)) ? 1 : 0;
		}
	}
	OP_IMPLEMENT_FMT(slti)
	{
		op_reg_ts_is16_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//set on less than immediate unsigned
	OP_IMPLEMENT(sltiu)
	{
		if (!rt_is_r0(opVal))
		{
			reg_rt(pCpu, opVal) = (reg_rs(pCpu, opVal) < op_imm_16(opVal)) ? 1 : 0;
		}
	}
	OP_IMPLEMENT_FMT(sltiu)
	{
		op_reg_ts_iu16_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//and immediate
	OP_IMPLEMENT(andi)
	{
		if (!rt_is_r0(opVal))
		{
			reg_rt(pCpu, opVal) = reg_rs(pCpu, opVal) & op_imm_16(opVal);
		}
	}
	OP_IMPLEMENT_FMT(andi)
	{
		op_reg_ts_iu16_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//or immediate
	OP_IMPLEMENT(ori)
	{
		if (!rt_is_r0(opVal))
		{
			reg_rt(pCpu, opVal) = reg_rs(pCpu, opVal) | op_imm_16(opVal);
		}
	}
	OP_IMPLEMENT_FMT(ori)
	{
		op_reg_ts_iu16_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//xor immediate
	OP_IMPLEMENT(xori)
	{
		if (!rt_is_r0(opVal))
		{
			reg_rt(pCpu, opVal) = reg_rs(pCpu, opVal) ^ op_imm_16(opVal);
		}
	}
	OP_IMPLEMENT_FMT(xori)
	{
		op_reg_ts_iu16_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//load upper immediate
	OP_IMPLEMENT(lui)
	{
		if (!rt_is_r0(opVal))
		{
			reg_rt(pCpu, opVal) = (op_imm_16(opVal) << 16);
		}
	}
	OP_IMPLEMENT_FMT(lui)
	{
		NoeSPrintF(pStringOut, CNoeR3000A::skMaximumOpStringLength, "%s%s, %i",
			string_for_op(pHandlerObject), reg_string_rt(opVal), op_imm_16(opVal));
	}

	//coprocessor 0 op
	OP_IMPLEMENT(cop0)
	{
		const SR3000ADefaultOpHandler &opHandler = CNoeR3000A::mspCOP0OpHandlers[CNoeR3000A::GetInstructionRS(opVal)];
		opHandler.mpOpHandler(pCpu, opVal, &opHandler);
	}
	OP_IMPLEMENT_FMT(cop0)
	{
		const SR3000ADefaultOpHandler &opHandler = CNoeR3000A::mspCOP0OpHandlers[CNoeR3000A::GetInstructionRS(opVal)];
		opHandler.mpOpStringFormat(pStringOut, pCpu, opVal, opAddr, &opHandler);
	}

	//load byte
	OP_IMPLEMENT(lb)
	{
		const int8_t readVal = pCpu->ReadMemory8(reg_rs(pCpu, opVal) + op_imm_s16(opVal));
		pCpu->LoadGPR(CNoeR3000A::EGPR(CNoeR3000A::GetInstructionRT(opVal)), opVal, int32_t(readVal));
	}
	OP_IMPLEMENT_FMT(lb)
	{
		load_store_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//load half-word
	OP_IMPLEMENT(lh)
	{
		const int16_t readVal = pCpu->ReadMemory16(reg_rs(pCpu, opVal) + op_imm_s16(opVal));
		pCpu->LoadGPR(CNoeR3000A::EGPR(CNoeR3000A::GetInstructionRT(opVal)), opVal, int32_t(readVal));
	}
	OP_IMPLEMENT_FMT(lh)
	{
		load_store_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//load unaligned word left
	OP_IMPLEMENT(lwl)
	{
		const uint32_t readAddr = reg_rs(pCpu, opVal) + op_imm_s16(opVal);
		const uint32_t readVal = pCpu->ReadMemory32((readAddr & ~3));
		const uint32_t unalignedShift = ((3 - (readAddr & 3)) << 3);
		const uint32_t rtMask = (1 << unalignedShift) - 1;

		uint32_t &rt = reg_rt(pCpu, opVal);
		const uint32_t newRegVal = (rt & rtMask) | (readVal << unalignedShift);
		pCpu->LoadGPR(CNoeR3000A::EGPR(CNoeR3000A::GetInstructionRT(opVal)), opVal, newRegVal);
	}
	OP_IMPLEMENT_FMT(lwl)
	{
		load_store_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//load word
	OP_IMPLEMENT(lw)
	{
		const uint32_t readVal = pCpu->ReadMemory32(reg_rs(pCpu, opVal) + op_imm_s16(opVal));
		pCpu->LoadGPR(CNoeR3000A::EGPR(CNoeR3000A::GetInstructionRT(opVal)), opVal, readVal);
	}
	OP_IMPLEMENT_FMT(lw)
	{
		load_store_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//load byte unsigned
	OP_IMPLEMENT(lbu)
	{
		const uint8_t readVal = pCpu->ReadMemory8(reg_rs(pCpu, opVal) + op_imm_s16(opVal));
		pCpu->LoadGPR(CNoeR3000A::EGPR(CNoeR3000A::GetInstructionRT(opVal)), opVal, readVal);
	}
	OP_IMPLEMENT_FMT(lbu)
	{
		load_store_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//load half-word unsigned
	OP_IMPLEMENT(lhu)
	{
		const uint16_t readVal = pCpu->ReadMemory16(reg_rs(pCpu, opVal) + op_imm_s16(opVal));
		pCpu->LoadGPR(CNoeR3000A::EGPR(CNoeR3000A::GetInstructionRT(opVal)), opVal, readVal);
	}
	OP_IMPLEMENT_FMT(lhu)
	{
		load_store_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//load unaligned word right
	OP_IMPLEMENT(lwr)
	{
		const uint32_t readAddr = reg_rs(pCpu, opVal) + op_imm_s16(opVal);
		const uint32_t readVal = pCpu->ReadMemory32((readAddr & ~3));
		const uint32_t unalignedShift = ((readAddr & 3) << 3);
		const uint32_t rtMask = (((1 << unalignedShift) - 1) << (32 - unalignedShift));

		uint32_t &rt = reg_rt(pCpu, opVal);
		const uint32_t newRegVal = (rt & rtMask) | (readVal >> unalignedShift);
		pCpu->LoadGPR(CNoeR3000A::EGPR(CNoeR3000A::GetInstructionRT(opVal)), opVal, newRegVal);
	}
	OP_IMPLEMENT_FMT(lwr)
	{
		load_store_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//store byte
	OP_IMPLEMENT(sb)
	{
		pCpu->WriteMemory8(reg_rs(pCpu, opVal) + op_imm_s16(opVal), reinterpret_cast<uint8_t &>(reg_rt(pCpu, opVal)));
	}
	OP_IMPLEMENT_FMT(sb)
	{
		load_store_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//store half-word
	OP_IMPLEMENT(sh)
	{
		pCpu->WriteMemory16(reg_rs(pCpu, opVal) + op_imm_s16(opVal), reinterpret_cast<uint16_t &>(reg_rt(pCpu, opVal)));
	}
	OP_IMPLEMENT_FMT(sh)
	{
		load_store_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//store unaligned word left
	OP_IMPLEMENT(swl)
	{
		const uint32_t addr = reg_rs(pCpu, opVal) + op_imm_s16(opVal);
		const uint32_t unalignedShift = ((3 - (addr & 3)) << 3);
		const uint32_t memMask = (((1 << unalignedShift) - 1) << (32 - unalignedShift));
		const uint32_t memVal = pCpu->ReadMemory32((addr & ~3));

		pCpu->WriteMemory32((addr & ~3), (reg_rt(pCpu, opVal) >> unalignedShift) | (memVal & memMask));
	}
	OP_IMPLEMENT_FMT(swl)
	{
		load_store_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//store word
	OP_IMPLEMENT(sw)
	{
		pCpu->WriteMemory32(reg_rs(pCpu, opVal) + op_imm_s16(opVal), reg_rt(pCpu, opVal));
	}
	OP_IMPLEMENT_FMT(sw)
	{
		load_store_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//store unaligned word right
	OP_IMPLEMENT(swr)
	{
		const uint32_t addr = reg_rs(pCpu, opVal) + op_imm_s16(opVal);
		const uint32_t unalignedShift = ((addr & 3) << 3);
		const uint32_t memMask = (1 << unalignedShift) - 1;
		const uint32_t memVal = pCpu->ReadMemory32((addr & ~3));

		pCpu->WriteMemory32((addr & ~3), (reg_rt(pCpu, opVal) << unalignedShift) | (memVal & memMask));
	}
	OP_IMPLEMENT_FMT(swr)
	{
		load_store_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//shift left logical
	OP_IMPLEMENT(sll)
	{
		if (!rd_is_r0(opVal))
		{
			reg_rd(pCpu, opVal) = (reg_rt(pCpu, opVal) << CNoeR3000A::GetInstructionSA(opVal));
		}
	}
	OP_IMPLEMENT_FMT(sll)
	{
		op_reg_dt_sa_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//shift right logical
	OP_IMPLEMENT(srl)
	{
		if (!rd_is_r0(opVal))
		{
			reg_rd(pCpu, opVal) = (reg_rt(pCpu, opVal) >> CNoeR3000A::GetInstructionSA(opVal));
		}
	}
	OP_IMPLEMENT_FMT(srl)
	{
		op_reg_dt_sa_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//shift right arithmetic
	OP_IMPLEMENT(sra)
	{
		if (!rd_is_r0(opVal))
		{
			reinterpret_cast<int32_t &>(reg_rd(pCpu, opVal)) = (int32_t(reg_rt(pCpu, opVal)) >> CNoeR3000A::GetInstructionSA(opVal));
		}
	}
	OP_IMPLEMENT_FMT(sra)
	{
		op_reg_dt_sa_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//shift left logical variable
	OP_IMPLEMENT(sllv)
	{
		if (!rd_is_r0(opVal))
		{
			reg_rd(pCpu, opVal) = (reg_rt(pCpu, opVal) << reg_rs(pCpu, opVal));
		}
	}
	OP_IMPLEMENT_FMT(sllv)
	{
		op_reg_dts_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//shift right logical variable
	OP_IMPLEMENT(srlv)
	{
		if (!rd_is_r0(opVal))
		{
			reg_rd(pCpu, opVal) = (reg_rt(pCpu, opVal) >> reg_rs(pCpu, opVal));
		}
	}
	OP_IMPLEMENT_FMT(srlv)
	{
		op_reg_dts_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//shift right arithmetic variable
	OP_IMPLEMENT(srav)
	{
		if (!rd_is_r0(opVal))
		{
			reinterpret_cast<int32_t &>(reg_rd(pCpu, opVal)) = (int32_t(reg_rt(pCpu, opVal)) >> reg_rs(pCpu, opVal));
		}
	}
	OP_IMPLEMENT_FMT(srav)
	{
		op_reg_dts_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//move from hi
	OP_IMPLEMENT(mfhi)
	{
		if (!rd_is_r0(opVal))
		{
			reg_rd(pCpu, opVal) = pCpu->mpIR[CNoeR3000A::kIR_Hi];
		}
	}
	OP_IMPLEMENT_FMT(mfhi)
	{
		op_reg_d_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//move to hi
	OP_IMPLEMENT(mthi)
	{
		pCpu->mpIR[CNoeR3000A::kIR_Hi] = reg_rd(pCpu, opVal);
	}
	OP_IMPLEMENT_FMT(mthi)
	{
		op_reg_d_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//move from lo
	OP_IMPLEMENT(mflo)
	{
		if (!rd_is_r0(opVal))
		{
			reg_rd(pCpu, opVal) = pCpu->mpIR[CNoeR3000A::kIR_Lo];
		}
	}
	OP_IMPLEMENT_FMT(mflo)
	{
		op_reg_d_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//move to lo
	OP_IMPLEMENT(mtlo)
	{
		pCpu->mpIR[CNoeR3000A::kIR_Lo] = reg_rd(pCpu, opVal);
	}
	OP_IMPLEMENT_FMT(mtlo)
	{
		op_reg_d_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//multiply
	OP_IMPLEMENT(mult)
	{
		const uint64_t result = uint64_t(int64_t(reinterpret_cast<int32_t &>(reg_rs(pCpu, opVal))) *
		                                 int64_t(reinterpret_cast<int32_t &>(reg_rt(pCpu, opVal))));
		pCpu->mpIR[CNoeR3000A::kIR_Lo] = uint32_t(result);
		pCpu->mpIR[CNoeR3000A::kIR_Hi] = uint32_t(result >> 32);
	}
	OP_IMPLEMENT_FMT(mult)
	{
		op_reg_st_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//multiply unsigned
	OP_IMPLEMENT(multu)
	{
		const uint64_t result = uint64_t(reg_rs(pCpu, opVal)) * uint64_t(reg_rt(pCpu, opVal));
		pCpu->mpIR[CNoeR3000A::kIR_Lo] = uint32_t(result);
		pCpu->mpIR[CNoeR3000A::kIR_Hi] = uint32_t(result >> 32);
	}
	OP_IMPLEMENT_FMT(multu)
	{
		op_reg_st_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//divide
	OP_IMPLEMENT(div)
	{
		if (reg_rt(pCpu, opVal) != 0)
		{
			pCpu->mpIR[CNoeR3000A::kIR_Lo] = uint32_t(reinterpret_cast<int32_t &>(reg_rs(pCpu, opVal)) / reinterpret_cast<int32_t &>(reg_rt(pCpu, opVal)));
			pCpu->mpIR[CNoeR3000A::kIR_Hi] = uint32_t(reinterpret_cast<int32_t &>(reg_rs(pCpu, opVal)) % reinterpret_cast<int32_t &>(reg_rt(pCpu, opVal)));
		}
		else
		{
			SetDivideByZeroResult(pCpu, reg_rs(pCpu, opVal));
		}
	}
	OP_IMPLEMENT_FMT(div)
	{
		op_reg_st_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//divide unsigned
	OP_IMPLEMENT(divu)
	{
		if (reg_rt(pCpu, opVal) != 0)
		{
			pCpu->mpIR[CNoeR3000A::kIR_Lo] = reg_rs(pCpu, opVal) / reg_rt(pCpu, opVal);
			pCpu->mpIR[CNoeR3000A::kIR_Hi] = reg_rs(pCpu, opVal) % reg_rt(pCpu, opVal);
		}
		else
		{
			SetDivideByZeroResult(pCpu, reg_rs(pCpu, opVal));
		}
	}
	OP_IMPLEMENT_FMT(divu)
	{
		op_reg_st_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//add
	OP_IMPLEMENT(add)
	{
		if (!rd_is_r0(opVal))
		{
			reg_rd(pCpu, opVal) = reg_rs(pCpu, opVal) + reg_rt(pCpu, opVal);
		}
	}
	OP_IMPLEMENT_FMT(add)
	{
		op_reg_dst_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//add unsigned
	OP_IMPLEMENT(addu)
	{
		add(pCpu, opVal, pHandlerObject);
	}
	OP_IMPLEMENT_FMT(addu)
	{
		add_format(pStringOut, pCpu, opVal, opAddr, pHandlerObject);
	}

	//subtract
	OP_IMPLEMENT(sub)
	{
		if (!rd_is_r0(opVal))
		{
			reg_rd(pCpu, opVal) = reg_rs(pCpu, opVal) - reg_rt(pCpu, opVal);
		}
	}
	OP_IMPLEMENT_FMT(sub)
	{
		op_reg_dst_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//subtract unsigned
	OP_IMPLEMENT(subu)
	{
		sub(pCpu, opVal, pHandlerObject);
	}
	OP_IMPLEMENT_FMT(subu)
	{
		sub_format(pStringOut, pCpu, opVal, opAddr, pHandlerObject);
	}

	//and
	OP_IMPLEMENT(and)
	{
		if (!rd_is_r0(opVal))
		{
			reg_rd(pCpu, opVal) = reg_rs(pCpu, opVal) & reg_rt(pCpu, opVal);
		}
	}
	OP_IMPLEMENT_FMT(and)
	{
		op_reg_dst_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//or
	OP_IMPLEMENT(or)
	{
		if (!rd_is_r0(opVal))
		{
			reg_rd(pCpu, opVal) = reg_rs(pCpu, opVal) | reg_rt(pCpu, opVal);
		}
	}
	OP_IMPLEMENT_FMT(or)
	{
		op_reg_dst_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//xor
	OP_IMPLEMENT(xor)
	{
		if (!rd_is_r0(opVal))
		{
			reg_rd(pCpu, opVal) = reg_rs(pCpu, opVal) ^ reg_rt(pCpu, opVal);
		}
	}
	OP_IMPLEMENT_FMT(xor)
	{
		op_reg_dst_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//nor
	OP_IMPLEMENT(nor)
	{
		if (!rd_is_r0(opVal))
		{
			reg_rd(pCpu, opVal) = ~(reg_rs(pCpu, opVal) | reg_rt(pCpu, opVal));
		}
	}
	OP_IMPLEMENT_FMT(nor)
	{
		op_reg_dst_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//set on less than
	OP_IMPLEMENT(slt)
	{
		if (!rd_is_r0(opVal))
		{
			reg_rd(pCpu, opVal) = (int32_t(reg_rs(pCpu, opVal)) < int32_t(reg_rt(pCpu, opVal))) ? 1 : 0;
		}
	}
	OP_IMPLEMENT_FMT(slt)
	{
		op_reg_dst_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//set on less than unsigned
	OP_IMPLEMENT(sltu)
	{
		if (!rd_is_r0(opVal))
		{
			reg_rd(pCpu, opVal) = (reg_rs(pCpu, opVal) < reg_rt(pCpu, opVal)) ? 1 : 0;
		}
	}
	OP_IMPLEMENT_FMT(sltu)
	{
		op_reg_dst_string_format(pStringOut, pCpu, opVal, pHandlerObject);
	}

	//branch if == rt
	OP_IMPLEMENT(beq)
	{
		if (reg_rs(pCpu, opVal) == reg_rt(pCpu, opVal))
		{
			TakeConditionalBranch(pCpu, opVal);
		}
	}
	OP_IMPLEMENT_FMT(beq)
	{
		conditional_branch_reg_string_format(pStringOut, pCpu, opVal, opAddr, pHandlerObject);
	}

	//branch if != rt
	OP_IMPLEMENT(bne)
	{
		if (reg_rs(pCpu, opVal) != reg_rt(pCpu, opVal))
		{
			TakeConditionalBranch(pCpu, opVal);
		}
	}
	OP_IMPLEMENT_FMT(bne)
	{
		conditional_branch_reg_string_format(pStringOut, pCpu, opVal, opAddr, pHandlerObject);
	}

	//branch if <= 0
	OP_IMPLEMENT(blez)
	{
		if (reinterpret_cast<int32_t &>(reg_rs(pCpu, opVal)) <= 0)
		{
			TakeConditionalBranch(pCpu, opVal);
		}
	}
	OP_IMPLEMENT_FMT(blez)
	{
		conditional_branch_string_format(pStringOut, pCpu, opVal, opAddr, pHandlerObject);
	}

	//branch if > 0
	OP_IMPLEMENT(bgtz)
	{
		if (reinterpret_cast<int32_t &>(reg_rs(pCpu, opVal)) > 0)
		{
			TakeConditionalBranch(pCpu, opVal);
		}
	}
	OP_IMPLEMENT_FMT(bgtz)
	{
		conditional_branch_string_format(pStringOut, pCpu, opVal, opAddr, pHandlerObject);
	}

	//branch if < 0
	OP_IMPLEMENT(bltz)
	{
		if (reinterpret_cast<int32_t &>(reg_rs(pCpu, opVal)) < 0)
		{
			TakeConditionalBranch(pCpu, opVal);
		}
	}
	OP_IMPLEMENT_FMT(bltz)
	{
		conditional_branch_string_format(pStringOut, pCpu, opVal, opAddr, pHandlerObject);
	}

	//branch if >= 0
	OP_IMPLEMENT(bgez)
	{
		if (reinterpret_cast<int32_t &>(reg_rs(pCpu, opVal)) >= 0)
		{
			TakeConditionalBranch(pCpu, opVal);
		}
	}
	OP_IMPLEMENT_FMT(bgez)
	{
		conditional_branch_string_format(pStringOut, pCpu, opVal, opAddr, pHandlerObject);
	}

	//branch if < 0 and link
	OP_IMPLEMENT(bltzal)
	{
		if (reinterpret_cast<int32_t &>(reg_rs(pCpu, opVal)) < 0)
		{
			SetReturnAddress(pCpu);
			TakeConditionalBranch(pCpu, opVal);
		}
	}
	OP_IMPLEMENT_FMT(bltzal)
	{
		conditional_branch_string_format(pStringOut, pCpu, opVal, opAddr, pHandlerObject);
	}

	//branch if >= 0 and link
	OP_IMPLEMENT(bgezal)
	{
		if (reinterpret_cast<int32_t &>(reg_rs(pCpu, opVal)) >= 0)
		{
			SetReturnAddress(pCpu);
			TakeConditionalBranch(pCpu, opVal);
		}
	}
	OP_IMPLEMENT_FMT(bgezal)
	{
		conditional_branch_string_format(pStringOut, pCpu, opVal, opAddr, pHandlerObject);
	}

	//move from cop0 register
	OP_IMPLEMENT(mfc0)
	{
		if (!rt_is_r0(opVal))
		{
			reg_rt(pCpu, opVal) = pCpu->C0R()[CNoeR3000A::GetInstructionRD(opVal)];
		}
	}
	OP_IMPLEMENT_FMT(mfc0)
	{
		NoeSPrintF(pStringOut, CNoeR3000A::skMaximumOpStringLength, "%s%s, %s",
			string_for_op(pHandlerObject), reg_string_rt(opVal), cop0_reg_string(CNoeR3000A::GetInstructionRD(opVal)));
	}
	//move from cop0 register
	OP_IMPLEMENT(cfc0)
	{
		mfc0(pCpu, opVal, pHandlerObject);
	}
	OP_IMPLEMENT_FMT(cfc0)
	{
		mfc0_format(pStringOut, pCpu, opVal, opAddr, pHandlerObject);
	}

	//move to cop0 register
	OP_IMPLEMENT(mtc0)
	{
		pCpu->mpWriteCOP0Reg(pCpu, CNoeR3000A::EC0R(CNoeR3000A::GetInstructionRD(opVal)), reg_rt(pCpu, opVal));
	}
	OP_IMPLEMENT_FMT(mtc0)
	{
		NoeSPrintF(pStringOut, CNoeR3000A::skMaximumOpStringLength, "%s%s, %s",
			string_for_op(pHandlerObject), reg_string_rt(opVal), cop0_reg_string(CNoeR3000A::GetInstructionRD(opVal)));
	}
	//move to cop0 register
	OP_IMPLEMENT(ctc0)
	{
		mtc0(pCpu, opVal, pHandlerObject);
	}
	OP_IMPLEMENT_FMT(ctc0)
	{
		mtc0_format(pStringOut, pCpu, opVal, opAddr, pHandlerObject);
	}

	//undo shift done at exception entry time
	OP_IMPLEMENT(rfe)
	{
		const uint32_t kStatusMask = (1 << 6) - 1;
		const uint32_t oldStatus = pCpu->C0R()[CNoeR3000A::kC0R_Status];
		const uint32_t newStatus = (oldStatus & ~kStatusMask) | ((oldStatus & kStatusMask) >> 2);
		pCpu->mpWriteCOP0Reg(pCpu, CNoeR3000A::kC0R_Status, newStatus);
	}
	OP_IMPLEMENT_FMT(rfe)
	{
		NoeSPrintF(pStringOut, CNoeR3000A::skMaximumOpStringLength, "rfe");
	}

private:
	static void TakeConditionalBranch(CNoeR3000A *pCpu, const uint32_t opVal)
	{
		pCpu->TakeBranch(pCpu->mpIR[CNoeR3000A::kIR_PC] + (op_imm_s16(opVal) << 2));
	}

	static void SetReturnAddress(CNoeR3000A *pCpu)
	{
		pCpu->mpGPR[CNoeR3000A::kGPR_RA] = pCpu->mpIR[CNoeR3000A::kIR_PC] + CNoeR3000A::skInstructionSize;
	}

	static void SetDivideByZeroResult(CNoeR3000A *pCpu, const uint32_t eVal)
	{
		//should check on hardware
		pCpu->mpIR[CNoeR3000A::kIR_Lo] = UINT_MAX;
		pCpu->mpIR[CNoeR3000A::kIR_Hi] = eVal;
	}

	CNoeR3000AOps(); //should never be instantiated
};

SR3000ADefaultOpHandler CNoeR3000A::mspDefaultOpHandlers[CNoeR3000A::skInstructionOpMaxCount] =
{
	OP_OB(table2), OP_OB(cbranch), OP_OB(j), OP_OB(jal), OP_OB(beq), OP_OB(bne), OP_OB(blez), OP_OB(bgtz),
	OP_OB(addi), OP_OB(addiu), OP_OB(slti), OP_OB(sltiu), OP_OB(andi), OP_OB(ori), OP_OB(xori), OP_OB(lui),
	OP_OB(cop0), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid),
	OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid),
	OP_OB(lb), OP_OB(lh), OP_OB(lwl), OP_OB(lw), OP_OB(lbu), OP_OB(lhu), OP_OB(lwr), OP_OB(invalid),
	OP_OB(sb), OP_OB(sh), OP_OB(swl), OP_OB(sw), OP_OB(invalid), OP_OB(invalid), OP_OB(swr), OP_OB(invalid),
	OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid),
	OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid)
};

SR3000ADefaultOpHandler CNoeR3000A::mspTable2OpHandlers[CNoeR3000A::skInstructionOpMaxCount] =
{
	OP_OB(sll), OP_OB(invalid), OP_OB(srl), OP_OB(sra), OP_OB(sllv), OP_OB(invalid), OP_OB(srlv), OP_OB(srav),
	OP_OB(jr), OP_OB(jalr), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid)/*syscall*/, OP_OB(invalid)/*break*/, OP_OB(invalid), OP_OB(invalid),
	OP_OB(mfhi), OP_OB(mthi), OP_OB(mflo), OP_OB(mtlo), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid),
	OP_OB(mult), OP_OB(multu), OP_OB(div), OP_OB(divu), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid),
	OP_OB(add), OP_OB(addu), OP_OB(sub), OP_OB(subu), OP_OB(and), OP_OB(or), OP_OB(xor), OP_OB(nor),
	OP_OB(invalid), OP_OB(invalid), OP_OB(slt), OP_OB(sltu), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid),
	OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid),
	OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid)
};

SR3000ADefaultOpHandler CNoeR3000A::mspCBranchOpHandlers[CNoeR3000A::skInstructionRegMaxCount] =
{
	OP_OB(bltz), OP_OB(bgez), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid),
	OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid),
	OP_OB(bltzal), OP_OB(bgezal), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid),
	OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid)
};

SR3000ADefaultOpHandler CNoeR3000A::mspCOP0OpHandlers[CNoeR3000A::skInstructionRegMaxCount] =
{
	OP_OB(mfc0), OP_OB(invalid), OP_OB(cfc0), OP_OB(invalid), OP_OB(mtc0), OP_OB(invalid), OP_OB(ctc0), OP_OB(invalid),
	OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid),
	OP_OB(rfe), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid),
	OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid), OP_OB(invalid)
};

CNoeR3000A::CNoeR3000A()
	: mpUserData(NULL)
#if NOER3000A_DEBUGGER()
	, mDebugger()
#endif
{
	SetDefaults();
	Reset();

#if NOER3000A_DEBUGGER()
	mDebugger.SetUserData(this);

	mDebugger.SetDebuggerReadData(debugger_read_data);
	mDebugger.SetDebuggerWriteData(debugger_write_data);

	mDebugger.SetDebuggerReadReg(debugger_read_reg);
	mDebugger.SetDebuggerWriteReg(debugger_write_reg);

	mDebugger.SetDebuggerDisassemble(debugger_disassemble);

	const NNoeDebugProtocol::SContext debugContext("MIPS_R3000A", skInstructionSize, 0, NNoeDebugProtocol::skRegCountAll);
	mDebugger.SetContexts(&debugContext, 1);

	//no need to keep track of register indices, as we implicitly associate based on added order/range
	NoeAssert(mDebugger.GetRegisterCount() == 0);
	for (uint32_t gprIndex = 0; gprIndex < CNoeR3000A::kGPR_Count; ++gprIndex)
	{
		mDebugger.AddRegister(skGPRNameForIndex[gprIndex], sizeof(uint32_t));
	}
	for (uint32_t irIndex = 0; irIndex < CNoeR3000A::kIR_Count; ++irIndex)
	{
		mDebugger.AddRegister(skIRNameForIndex[irIndex], sizeof(uint32_t),
			(irIndex == CNoeR3000A::kIR_PC) ? NNoeDebugProtocol::kRF_PC : NNoeDebugProtocol::kRF_None);
	}
	for (uint32_t c0rIndex = 0; c0rIndex < CNoeR3000A::kC0R_Count; ++c0rIndex)
	{
		mDebugger.AddRegister(cop0_reg_string(c0rIndex), sizeof(uint32_t));
	}
#endif
}

void CNoeR3000A::ExecuteCycles(const uint64_t cycleCount)
{
	const uint64_t targetCycleCount = mCyclesExecuted + cycleCount;
	NoeAssert(targetCycleCount > mCyclesExecuted);
	while (mCyclesExecuted < targetCycleCount)
	{
		ReadAndExecuteOp();
	}
}

void CNoeR3000A::ConsumeCycles(const int64_t cycleCount)
{
	mCyclesExecuted += cycleCount;
	if (mpOnCyclesConsumed)
	{
		mpOnCyclesConsumed(this, cycleCount);
	}
}

void CNoeR3000A::CheckDelayedOperations()
{
	//check for n-cycle delayed branches
	if (mDelayBranch.mTargetCycle && mDelayBranch.mTargetCycle <= mCyclesExecuted)
	{
		mpIR[kIR_PC] = mDelayBranch.mAddr;
		mDelayBranch.mTargetCycle = 0;

		if (mpOnBranchTaken)
		{
			mpOnBranchTaken(this, mDelayBranch.mAddr);
		}
	}

	//check for n-cycle delayed loads
	if (mDelayGPRLoadMask)
	{
		uint32_t bitStart, bitEndPlusOne;
		NoeGetFirstLastBitSet64(&bitStart, &bitEndPlusOne, mDelayGPRLoadMask);
		for (uint32_t currentBitIndex = bitStart; currentBitIndex < bitEndPlusOne; ++currentBitIndex)
		{
			const uint64_t currentBit = (1ULL << uint64_t(currentBitIndex));
			if (mDelayGPRLoadMask & currentBit)
			{
				const SDelayGPRLoad &delayLoad = mDelayGPRLoad[currentBitIndex];
				if (delayLoad.mTargetCycle <= mCyclesExecuted)
				{
					mpGPR[currentBitIndex] = delayLoad.mRegVal;
					mDelayGPRLoadMask &= ~currentBit;
				}
			}
		}
	}
}

void CNoeR3000A::AddOpHandler(const SOpHandler &opHandler)
{
	const uint32_t op = GetInstructionOp(opHandler.mOpVal);
	mOpHandlers[op].push_back(opHandler);
}

void CNoeR3000A::SetDefaults()
{
	for (int32_t opIndex = 0; opIndex < skInstructionOpMaxCount; ++opIndex)
	{
		mOpHandlers[opIndex].clear();
	}

	SetReadMemory8(NULL);
	SetReadMemory16(NULL);
	SetReadMemory32(NULL);
	SetReadInstruction(NULL);
	SetWriteMemory8(NULL);
	SetWriteMemory16(NULL);
	SetWriteMemory32(NULL);

	SetWriteCOP0Reg(NULL);
	
	SetDelayLoadForOp(NULL);

	SetOnCyclesConsumed(NULL);
	SetOnOpExecuted(NULL);
	SetOnBranchBegin(NULL);
	SetOnBranchTaken(NULL);

	SetGPRMemory(NULL);
	SetIRMemory(NULL);
	SetC0RMemory(NULL);

	SetCyclesPerOp(skDefaultCyclesPerOp);

	SetDelayBranchCycles(skDefaultDelayBranchCycles);
	SetDelayLoadCycles(skDefaultDelayLoadCycles);
}

void CNoeR3000A::Reset()
{
	mCyclesExecuted = 0;
	mDelayBranch = SDelayBranch();
	mDelayGPRLoadMask = 0;

#if NOER3000A_DEBUGGER()
	mDebugger.OnReset();
#endif
}

void CNoeR3000A::SetReadMemory8(TReadMemory8 pReadMemory8)
{
	mpReadMemory8 = (pReadMemory8) ? pReadMemory8 : default_read_memory<uint8_t>;
}

void CNoeR3000A::SetReadMemory16(TReadMemory16 pReadMemory16)
{
	mpReadMemory16 = (pReadMemory16) ? pReadMemory16 : default_read_memory<uint16_t>;
}

void CNoeR3000A::SetReadMemory32(TReadMemory32 pReadMemory32)
{
	mpReadMemory32 = (pReadMemory32) ? pReadMemory32 : default_read_memory<uint32_t>;
}

void CNoeR3000A::SetReadInstruction(TReadInstruction pReadInstruction)
{
	mpReadInstruction = (pReadInstruction) ? pReadInstruction : default_read_memory<uint32_t>;
}

void CNoeR3000A::SetWriteMemory8(TWriteMemory8 pWriteMemory8)
{
	mpWriteMemory8 = (pWriteMemory8) ? pWriteMemory8 : default_write_memory<uint8_t>;
}

void CNoeR3000A::SetWriteMemory16(TWriteMemory16 pWriteMemory16)
{
	mpWriteMemory16 = (pWriteMemory16) ? pWriteMemory16 : default_write_memory<uint16_t>;
}

void CNoeR3000A::SetWriteMemory32(TWriteMemory32 pWriteMemory32)
{
	mpWriteMemory32 = (pWriteMemory32) ? pWriteMemory32 : default_write_memory<uint32_t>;
}

void CNoeR3000A::SetWriteCOP0Reg(TWriteCOP0Reg pWriteCOP0Reg)
{
	mpWriteCOP0Reg = (pWriteCOP0Reg) ? pWriteCOP0Reg : default_write_cop0_reg;
}

void CNoeR3000A::SetDelayLoadForOp(TDelayLoadForOp pDelayLoadForOp)
{
	mpDelayLoadForOp = pDelayLoadForOp;
}

void CNoeR3000A::SetOnCyclesConsumed(TOnCyclesConsumed pOnCyclesConsumed)
{
	mpOnCyclesConsumed = pOnCyclesConsumed;
}

void CNoeR3000A::SetOnOpExecuted(TOnOpExecuted pOnOpExecuted)
{
	mpOnOpExecuted = pOnOpExecuted;
}

void CNoeR3000A::SetOnBranchBegin(TOnBranch pOnBranchBegin)
{
	mpOnBranchBegin = pOnBranchBegin;
}

void CNoeR3000A::SetOnBranchTaken(TOnBranch pOnBranchTaken)
{
	mpOnBranchTaken = pOnBranchTaken;
}

void CNoeR3000A::SetGPRMemory(uint32_t *pGPR)
{
	mpGPR = (pGPR) ? pGPR : mDefaultGPR;
}

void CNoeR3000A::SetIRMemory(uint32_t *pIR)
{
	mpIR = (pIR) ? pIR : mDefaultIR;
}

void CNoeR3000A::SetC0RMemory(uint32_t *pC0R)
{
	mpC0R = (pC0R) ? pC0R : mDefaultC0R;
}

const char *CNoeR3000A::GetGPRName(const EGPR gprIndex)
{
	return skGPRNameForIndex[gprIndex];
}

void CNoeR3000A::ReadAndExecuteOp()
{
	const uint32_t opAddr = mpIR[kIR_PC];
	mpIR[kIR_Op] = ReadInstruction(opAddr);

#if NOER3000A_DEBUGGER()
	mDebugger.CheckInstructionBP(opAddr);

	//spin in place while suspended, waiting to receive a command to resume.
	//currently we don't explicitly support stepping in/over/out, so they'll just be
	//treated as single steps with this behavior.
	do
	{
		mDebugger.Update();
	} while (mDebugger.GetStatus() == NNoeDebugProtocol::kStatus_Halted);
#endif

	mpIR[kIR_PC] += skInstructionSize;
	//consume default number of cycles. usually it will be appropriate to make this 0,
	//and burn the appropriate number of cycles for actually reading the instruction,
	//particularly contingent upon whether the instruction was in a cache or not.
	ConsumeCycles(mCyclesPerOp);

	ExecuteOp(mpIR[kIR_Op], opAddr);

	NoeAssert(mpGPR[kGPR_Zero] == 0);
}

void CNoeR3000A::ExecuteOp(const uint32_t opVal, const uint32_t opAddr)
{
	//don't bother doing anything if this is a pure nop
	if (opVal != 0)
	{
		const uint32_t op = GetInstructionOp(opVal);
	
		//first see if we have any custom handlers for this op
		bool ranCustomHandler = false;
		const TOpHandlerList &handlers = mOpHandlers[op];
		for (TOpHandlerList::const_iterator it = handlers.begin(); it != handlers.end(); ++it)
		{
			const SOpHandler &handler = *it;
			if (handler.mOpVal == (opVal & handler.mOpMask) &&
				handler.mpOpHandler(this, opVal))
			{
				ranCustomHandler = true;
				break;
			}
		}

		//if nothing handled it, fall back to our table
		if (!ranCustomHandler)
		{
			const SR3000ADefaultOpHandler &opHandler = mspDefaultOpHandlers[op];
			opHandler.mpOpHandler(this, opVal, &opHandler);
		}
	}

	if (mpOnOpExecuted)
	{
		mpOnOpExecuted(this, opVal);
	}

	CheckDelayedOperations();
}

void CNoeR3000A::TakeBranch(const uint32_t addr)
{
	if (mpOnBranchBegin)
	{
		mpOnBranchBegin(this, addr);
	}

	if (mDelayBranchCycles)
	{
		mDelayBranch.mAddr = addr;
		mDelayBranch.mTargetCycle = mCyclesExecuted + mDelayBranchCycles;
	}
	else
	{
		//execute the delay slot in-place - even if we don't have a cycle-based delay set, this is a mandatory part of the architecture
		ReadAndExecuteOp();

		mpIR[kIR_PC] = addr;

		if (mpOnBranchTaken)
		{
			mpOnBranchTaken(this, addr);
		}
	}
}

void CNoeR3000A::LoadGPR(const EGPR gprIndex, const uint32_t opVal, const uint32_t regVal)
{
	if (gprIndex != kGPR_Zero)
	{
		const uint32_t delayLoadCycles = (mpDelayLoadForOp) ? mpDelayLoadForOp(this, opVal) : mDelayLoadCycles;
		if (!delayLoadCycles)
		{
			mpGPR[gprIndex] = regVal;
		}
		else
		{
			mDelayGPRLoad[gprIndex].mRegVal = regVal;
			mDelayGPRLoad[gprIndex].mTargetCycle = mCyclesExecuted + delayLoadCycles;
			mDelayGPRLoadMask |= (1ULL << gprIndex);
		}
	}
}

#if NOER3000A_DEBUGGER()
	#define DEBUGGER_CHECK_READ(addr, size, accessType) \
		if (accessType == kAT_Default) \
		{ \
			mDebugger.CheckDataReadBP(addr, size); \
		}
	#define DEBUGGER_CHECK_WRITE(addr, size, accessType) \
		if (accessType == kAT_Default) \
		{ \
			mDebugger.CheckDataWriteBP(addr, size); \
		}
#else
	#define DEBUGGER_CHECK_READ(addr, size, accessType)
	#define DEBUGGER_CHECK_WRITE(addr, size, accessType)
#endif

uint8_t CNoeR3000A::ReadMemory8(const uint32_t addr, const EAccessType accessType)
{
	DEBUGGER_CHECK_READ(addr, sizeof(uint8_t), accessType);
	return mpReadMemory8(this, addr, accessType);
}

uint16_t CNoeR3000A::ReadMemory16(const uint32_t addr, const EAccessType accessType)
{
	DEBUGGER_CHECK_READ(addr, sizeof(uint16_t), accessType);
	return mpReadMemory16(this, addr, accessType);
}

uint32_t CNoeR3000A::ReadMemory32(const uint32_t addr, const EAccessType accessType)
{
	DEBUGGER_CHECK_READ(addr, sizeof(uint32_t), accessType);
	return mpReadMemory32(this, addr, accessType);
}

uint32_t CNoeR3000A::ReadInstruction(const uint32_t addr, const EAccessType accessType)
{
	return mpReadInstruction(this, addr, accessType);
}

void CNoeR3000A::WriteMemory8(const uint32_t addr, const uint8_t memVal, const EAccessType accessType)
{
	DEBUGGER_CHECK_WRITE(addr, sizeof(uint8_t), accessType);
	mpWriteMemory8(this, addr, memVal, accessType);
}

void CNoeR3000A::WriteMemory16(const uint32_t addr, const uint16_t memVal, const EAccessType accessType)
{
	DEBUGGER_CHECK_WRITE(addr, sizeof(uint16_t), accessType);
	mpWriteMemory16(this, addr, memVal, accessType);
}

void CNoeR3000A::WriteMemory32(const uint32_t addr, const uint32_t memVal, const EAccessType accessType)
{
	DEBUGGER_CHECK_WRITE(addr, sizeof(uint32_t), accessType);
	mpWriteMemory32(this, addr, memVal, accessType);
}

void CNoeR3000A::FormatOpString(char *pStringOut, const uint32_t opVal, const uint32_t opAddr)
{
	if (opVal != 0)
	{
		const uint32_t op = GetInstructionOp(opVal);
	
		const TOpHandlerList &handlers = mOpHandlers[op];
		for (TOpHandlerList::const_iterator it = handlers.begin(); it != handlers.end(); ++it)
		{
			const SOpHandler &handler = *it;
			if (handler.mOpVal == (opVal & handler.mOpMask) &&
				handler.mpOpStringFormat &&
				handler.mpOpStringFormat(pStringOut, this, opVal))
			{
				return;
			}
		}

		const SR3000ADefaultOpHandler &opHandler = mspDefaultOpHandlers[op];
		opHandler.mpOpStringFormat(pStringOut, this, opVal, opAddr, &opHandler);
	}
	else
	{
		CNoeR3000AOps::nop_format(pStringOut, this, opVal, opAddr, NULL);
	}
}