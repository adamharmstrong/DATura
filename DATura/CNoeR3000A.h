//Displaying: CNoeR3000A.h

//===============================================================================
// Noesis R3000A CPU core
// (c) whenever Rich Whitehouse
// an easily-debuggable and pliable R3000A core.
// has been live-tested inside a PSX emulator with SotN, but not too extensively.
//===============================================================================

#pragma once

#if !defined(_CNOER3000A_H_)

#define _CNOER3000A_H_

#define NOER3000A_DEBUGGER() 0

#include "NoesisLocalAPI.h"

#if NOER3000A_DEBUGGER()
#include "CNoeDebugger.h"
#endif

struct SR3000ADefaultOpHandler;

class CNoeR3000A
{
	friend class CNoeR3000AOps;

public:
	static const uint32_t skInstructionSize = 4;

	static const uint32_t skInstructionOffsetOp = 26;
	static const uint32_t skInstructionOffsetOp2 = 0;
	static const uint32_t skInstructionOpBits = 6;
	static const uint32_t skInstructionOpMaxCount = (1 << skInstructionOpBits);

	static const uint32_t skInstructionOffsetRegSA = 6;
	static const uint32_t skInstructionOffsetRegRD = 11;
	static const uint32_t skInstructionOffsetRegRT = 16;
	static const uint32_t skInstructionOffsetRegRS = 21;
	static const uint32_t skInstructionRegBits = 5;
	static const uint32_t skInstructionRegMaxCount = (1 << skInstructionRegBits);

	static const uint32_t skInstructionJumpAddressBits = 26;
	static const uint32_t skInstructionJumpAddressPCMask = 0xF0000000;

	static const uint32_t skMaximumOpStringLength = 1024;
	static const uint32_t skMaximumOpNameLength = 8;

	enum EGPR
	{
		kGPR_Zero = 0, //r0
		kGPR_AT,       //r1
		kGPR_V0,       //r2
		kGPR_V1,       //r3
		kGPR_A0,       //r4
		kGPR_A1,       //r5
		kGPR_A2,       //r6
		kGPR_A3,       //r7
		kGPR_T0,       //r8
		kGPR_T1,       //r9
		kGPR_T2,       //r10
		kGPR_T3,       //r11
		kGPR_T4,       //r12
		kGPR_T5,       //r13
		kGPR_T6,       //r14
		kGPR_T7,       //r15
		kGPR_S0,       //r16
		kGPR_S1,       //r17
		kGPR_S2,       //r18
		kGPR_S3,       //r19
		kGPR_S4,       //r20
		kGPR_S5,       //r21
		kGPR_S6,       //r22
		kGPR_S7,       //r23
		kGPR_T8,       //r24
		kGPR_T9,       //r25
		kGPR_K0,       //r26
		kGPR_K1,       //r27
		kGPR_GP,       //r28
		kGPR_SP,       //r29
		kGPR_FP,       //r30 / s8
		kGPR_RA,       //r31
		kGPR_Count
	};

	enum EIR
	{
		kIR_Lo = 0,
		kIR_Hi,
		kIR_PC,
		kIR_Op,
		kIR_Count
	};

	enum EC0R
	{
		kC0R_Index = 0,
		kC0R_Random,
		kC0R_EntryLo,
		kC0R_C3,
		kC0R_Context,
		kC0R_C5,
		kC0R_C6,
		kC0R_C7,
		kC0R_VAddr,
		kC0R_C9,
		kC0R_EntryHi,
		kC0R_C11,
		kC0R_Status,
		kC0R_Cause,
		kC0R_EPC,
		kC0R_C15,
		kC0R_C16,
		kC0R_C17,
		kC0R_C18,
		kC0R_C19,
		kC0R_C20,
		kC0R_C21,
		kC0R_C22,
		kC0R_C23,
		kC0R_C24,
		kC0R_C25,
		kC0R_C26,
		kC0R_C27,
		kC0R_C28,
		kC0R_C29,
		kC0R_C30,
		kC0R_C31,
		kC0R_Count
	};

	enum EAccessType
	{
		kAT_Default = 0,
		kAT_External
	};

	typedef uint8_t (*TReadMemory8)(CNoeR3000A *pCpu, const uint32_t addr, const EAccessType accessType);
	typedef uint16_t (*TReadMemory16)(CNoeR3000A *pCpu, const uint32_t addr, const EAccessType accessType);
	typedef uint32_t (*TReadMemory32)(CNoeR3000A *pCpu, const uint32_t addr, const EAccessType accessType);
	typedef uint32_t (*TReadInstruction)(CNoeR3000A *pCpu, const uint32_t addr, const EAccessType accessType);
	typedef void (*TWriteMemory8)(CNoeR3000A *pCpu, const uint32_t addr, const uint8_t memVal, const EAccessType accessType);
	typedef void (*TWriteMemory16)(CNoeR3000A *pCpu, const uint32_t addr, const uint16_t memVal, const EAccessType accessType);
	typedef void (*TWriteMemory32)(CNoeR3000A *pCpu, const uint32_t addr, const uint32_t memVal, const EAccessType accessType);
	typedef void (*TWriteCOP0Reg)(CNoeR3000A *pCpu, const EC0R cop0Reg, const uint32_t regVal);
	typedef uint32_t (*TDelayLoadForOp)(CNoeR3000A *pCpu, const uint32_t opVal);
	typedef void (*TOnCyclesConsumed)(CNoeR3000A *pCpu, const uint64_t cycleCount);
	typedef void (*TOnOpExecuted)(CNoeR3000A *pCpu, const uint32_t opVal);
	typedef void (*TOnBranch)(CNoeR3000A *pCpu, const uint32_t addr);

	//should return false if the op was invalid
	typedef bool (*TOpHandlerFunction)(CNoeR3000A *pCpu, const uint32_t opVal);
	typedef bool (*TOpStringFormatFunction)(char *pStringOut, CNoeR3000A *pCpu, const uint32_t opVal);
	struct SOpHandler
	{
		explicit SOpHandler(const uint32_t opVal, const uint32_t opMask, TOpHandlerFunction pOpHandler, TOpStringFormatFunction pOpStringFormat)
			: mOpVal(opVal)
			, mOpMask(opMask)
			, mpOpHandler(pOpHandler)
			, mpOpStringFormat(pOpStringFormat)
		{
		}

		uint32_t mOpVal;
		uint32_t mOpMask;
		TOpHandlerFunction mpOpHandler;
		TOpStringFormatFunction mpOpStringFormat;
	};

	CNoeR3000A();

	void ExecuteCycles(const uint64_t cycleCount);
	void SetDefaults();
	void Reset();

	void ConsumeCycles(const int64_t cycleCount);

	void CheckDelayedOperations();

	uint32_t *GPR() { return mpGPR; }
	uint32_t *IR() { return mpIR; }
	uint32_t *C0R() { return mpC0R; }

	void AddOpHandler(const SOpHandler &opHandler);

	//pass NULL for SetRead*/SetWrite* functions to use defaults.
	void SetReadMemory8(TReadMemory8 pReadMemory8);
	void SetReadMemory16(TReadMemory16 pReadMemory16);
	void SetReadMemory32(TReadMemory32 pReadMemory32);
	void SetReadInstruction(TReadInstruction pReadInstruction);
	void SetWriteMemory8(TWriteMemory8 pWriteMemory8);
	void SetWriteMemory16(TWriteMemory16 pWriteMemory16);
	void SetWriteMemory32(TWriteMemory32 pWriteMemory32);

	void SetWriteCOP0Reg(TWriteCOP0Reg pWriteCOP0Reg);
	
	void SetDelayLoadForOp(TDelayLoadForOp pDelayLoadForOp);

	void SetOnCyclesConsumed(TOnCyclesConsumed pOnCyclesConsumed);
	void SetOnOpExecuted(TOnOpExecuted pOnOpExecuted);
	void SetOnBranchBegin(TOnBranch pOnBranchBegin);
	void SetOnBranchTaken(TOnBranch pOnBranchTaken);

	void SetGPRMemory(uint32_t *pGPR); //NULL to set to defaults
	void SetIRMemory(uint32_t *pIR); //NULL to set to defaults
	void SetC0RMemory(uint32_t *pC0R); //NULL to set to defaults

	uint8_t ReadMemory8(const uint32_t addr, const EAccessType accessType = kAT_Default);
	uint16_t ReadMemory16(const uint32_t addr, const EAccessType accessType = kAT_Default);
	uint32_t ReadMemory32(const uint32_t addr, const EAccessType accessType = kAT_Default);
	uint32_t ReadInstruction(const uint32_t addr, const EAccessType accessType = kAT_Default);
	void WriteMemory8(const uint32_t addr, const uint8_t memVal, const EAccessType accessType = kAT_Default);
	void WriteMemory16(const uint32_t addr, const uint16_t memVal, const EAccessType accessType = kAT_Default);
	void WriteMemory32(const uint32_t addr, const uint32_t memVal, const EAccessType accessType = kAT_Default);

	uint64_t GetCyclesPerOp() const { return mCyclesPerOp; }
	void SetCyclesPerOp(const uint64_t cyclesPerOp) { mCyclesPerOp = cyclesPerOp; }

	uint64_t GetCyclesExecuted() const { return mCyclesExecuted; }

	void SetDelayBranchCycles(const uint32_t delayBranchCycles) { mDelayBranchCycles = delayBranchCycles; }
	void SetDelayLoadCycles(const uint32_t delayLoadCycles) { mDelayLoadCycles = delayLoadCycles; }

	void *GetUserData() const { return mpUserData; }
	void SetUserData(void *pUserData) { mpUserData = pUserData; }

	static uint32_t GetInstructionOp(const uint32_t opVal) { return (opVal >> skInstructionOffsetOp) & ((1 << skInstructionOpBits) - 1); }
	static uint32_t GetInstructionOp2(const uint32_t opVal) { return (opVal >> skInstructionOffsetOp2) & ((1 << skInstructionOpBits) - 1); }
	static uint32_t GetInstructionSA(const uint32_t opVal) { return (opVal >> skInstructionOffsetRegSA) & ((1 << skInstructionRegBits) - 1); }
	static uint32_t GetInstructionRD(const uint32_t opVal) { return (opVal >> skInstructionOffsetRegRD) & ((1 << skInstructionRegBits) - 1); }
	static uint32_t GetInstructionRT(const uint32_t opVal) { return (opVal >> skInstructionOffsetRegRT) & ((1 << skInstructionRegBits) - 1); }
	static uint32_t GetInstructionRS(const uint32_t opVal) { return (opVal >> skInstructionOffsetRegRS) & ((1 << skInstructionRegBits) - 1); }
	static uint32_t GetInstructionJumpAddress(const uint32_t opVal) { return (opVal & ((1 << skInstructionJumpAddressBits) - 1)) << 2; }

	static const char *GetGPRName(const EGPR gprIndex);

	void FormatOpString(char *pStringOut, const uint32_t opVal, const uint32_t opAddr);

#if NOER3000A_DEBUGGER()
	CNoeDebugger &Debugger() { return mDebugger; }
#endif

private:
	void ReadAndExecuteOp();
	void ExecuteOp(const uint32_t opVal, const uint32_t opAddr);

	void TakeBranch(const uint32_t addr);

	void LoadGPR(const EGPR gprIndex, const uint32_t opVal, const uint32_t regVal);

	//registered handlers are checked first, and if nothing elects to handle an op, we fall back to the default handler table.
	//the default table may then reference into other tables depending on the op. this allows ops within the default table (and
	//within tables referenced by the default table) to be selectively overridden via masking as well.
	static SR3000ADefaultOpHandler mspDefaultOpHandlers[CNoeR3000A::skInstructionOpMaxCount];
	static SR3000ADefaultOpHandler mspTable2OpHandlers[CNoeR3000A::skInstructionOpMaxCount];
	static SR3000ADefaultOpHandler mspCBranchOpHandlers[CNoeR3000A::skInstructionRegMaxCount];
	static SR3000ADefaultOpHandler mspCOP0OpHandlers[CNoeR3000A::skInstructionRegMaxCount];

	typedef std::vector<SOpHandler> TOpHandlerList;
	//we allow custom op handlers stored as a list of possible handlers for each 6-bit opcode.
	//instruction masks are used to further narrow down which handler to select from within the list.
	TOpHandlerList mOpHandlers[skInstructionOpMaxCount];

	TReadMemory8 mpReadMemory8;
	TReadMemory16 mpReadMemory16;
	TReadMemory32 mpReadMemory32;
	TReadInstruction mpReadInstruction;
	TWriteMemory8 mpWriteMemory8;
	TWriteMemory16 mpWriteMemory16;
	TWriteMemory32 mpWriteMemory32;

	TWriteCOP0Reg mpWriteCOP0Reg;
	
	TDelayLoadForOp mpDelayLoadForOp;

	TOnCyclesConsumed mpOnCyclesConsumed;
	TOnOpExecuted mpOnOpExecuted;
	TOnBranch mpOnBranchBegin;
	TOnBranch mpOnBranchTaken;

	uint32_t *mpGPR;
	uint32_t *mpIR;
	uint32_t *mpC0R;

	uint64_t mCyclesPerOp;

	uint64_t mCyclesExecuted;

	struct SDelayBranch
	{
		SDelayBranch()
			: mAddr(0)
			, mTargetCycle(0)
		{
		}

		uint32_t mAddr;
		uint64_t mTargetCycle;
	};
	SDelayBranch mDelayBranch;

	struct SDelayGPRLoad
	{
		SDelayGPRLoad()
			: mRegVal(0)
			, mTargetCycle(0)
		{
		}

		uint32_t mRegVal;
		uint64_t mTargetCycle;
	};
	SDelayGPRLoad mDelayGPRLoad[kGPR_Count];
	uint64_t mDelayGPRLoadMask;

	uint32_t mDelayBranchCycles;
	uint32_t mDelayLoadCycles;

	void *mpUserData;

#if NOER3000A_DEBUGGER()
	CNoeDebugger mDebugger;
#endif

	//never access these directly, mpGPR and mpIR may optionally point at external register sets
	uint32_t mDefaultGPR[kGPR_Count];
	uint32_t mDefaultIR[kIR_Count];
	uint32_t mDefaultC0R[kC0R_Count];
};

#endif //_CNOER3000A_H_