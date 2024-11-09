/* 046267 Computer Architecture - HW #4 */

#include "core_api.h"
#include "sim_api.h"
#include <stdio.h>

//#include <stdlib.h>

typedef struct {

    bool isHalted;
    unsigned int timer;
    tcontext* regs;
    Instruction* currInst;
    uint32_t pc;
    bool idle;
    unsigned int th_id;
    unsigned int last_active;

}Thread;

typedef struct {
    unsigned int cycles;
    unsigned int instructions;
    unsigned int th_number;
    Thread* threads;
    bool blocked_mt;
    unsigned int load_latency;
    unsigned int store_latency;
    unsigned int overhead;
}Core;


Core* CoreInit(bool blocked)
{
    Core* core = (Core*)malloc(sizeof(Core));
    if(!core)
    {
        return NULL;
    }
    core->blocked_mt = blocked;
    core->th_number = SIM_GetThreadsNum();
    core->cycles = 0;
    core->instructions = 0;
    core->threads = (Thread*)malloc(sizeof(Thread) * core->th_number);
    if (blocked) core->overhead = SIM_GetSwitchCycles();
    else core->overhead = 0;
    core->load_latency = SIM_GetLoadLat();
    core->store_latency = SIM_GetStoreLat();
    if(!core->threads)
    {
        free(core);
        return NULL;
    }
    for (unsigned int i = 0; i < core->th_number ; ++i)
    {
        core->threads[i].isHalted = false;
        core->threads[i].timer = 0;
        core->threads[i].idle = 0;
        core->threads[i].currInst = NULL;
        core->threads[i].pc = 0;
        core->threads[i].last_active = 0;
        core->threads[i].regs = (tcontext*)malloc(sizeof(tcontext));
        if (!core->threads[i].regs) {
            for (int j = 0; j < i; ++j) {
                free(core->threads[j].regs);
            }
            free(core->threads);
            free(core);
            return NULL;
        }
        core->threads[i].currInst = (Instruction*)malloc(sizeof(Instruction));
        if (!core->threads[i].currInst) {
            for (int j = 0; j < i; ++j) {
                free(core->threads[j].currInst);
            }
            for (int j = 0; j < i; ++j) {
                free(core->threads[j].regs);
            }
            free(core->threads);
            free(core);
            return NULL;
        }

        for (int j = 0; j < REGS_COUNT; ++j)
        {
            core->threads[i].regs->reg[j] = 0;
        }

    }
    return core;


}

Core* bCore;
Core* fCore;

void CORE_BlockedMT() {
    bCore = CoreInit(true);
    bool contextSwitch = false;
    int currTID = 0; //current thread ID


    while (true) {
        SIM_MemInstRead(bCore->threads[currTID].pc, bCore->threads[currTID].currInst, currTID);
        contextSwitch = false;
        bCore->cycles++;
        bCore->instructions++;
        bCore->threads[currTID].last_active = bCore->cycles;
        bCore->threads[currTID].pc++;

        for (unsigned int i = 0; i < bCore->th_number; i++) {
            if (bCore->threads[i].isHalted) continue;
            if (1 >= bCore->threads[i].timer) {
                bCore->threads[i].timer = 0;
                bCore->threads[i].idle = false;
            } else {
                bCore->threads[i].timer -= 1;
            }
        }

        switch (bCore->threads[currTID].currInst->opcode) {
            case CMD_NOP:
                break;

            case CMD_ADD: {
                int destRegIndex = bCore->threads[currTID].currInst->dst_index;
                int regAIndex = bCore->threads[currTID].currInst->src1_index;
                int regBIndex = bCore->threads[currTID].currInst->src2_index_imm;
                int result =
                        bCore->threads[currTID].regs->reg[regAIndex] + bCore->threads[currTID].regs->reg[regBIndex];
                bCore->threads[currTID].regs->reg[destRegIndex] = result;
                break;
            }
            case CMD_SUB: {
                int destRegIndex = bCore->threads[currTID].currInst->dst_index;
                int regAIndex = bCore->threads[currTID].currInst->src1_index;
                int regBIndex = bCore->threads[currTID].currInst->src2_index_imm;
                int result =
                        bCore->threads[currTID].regs->reg[regAIndex] - bCore->threads[currTID].regs->reg[regBIndex];
                bCore->threads[currTID].regs->reg[destRegIndex] = result;
                break;
            }
            case CMD_ADDI: {
                int destRegIndex = bCore->threads[currTID].currInst->dst_index;
                int regAIndex = bCore->threads[currTID].currInst->src1_index;
                int Immediate = bCore->threads[currTID].currInst->src2_index_imm;
                int result = bCore->threads[currTID].regs->reg[regAIndex] + Immediate;
                bCore->threads[currTID].regs->reg[destRegIndex] = result;
                break;
            }
            case CMD_SUBI: {
                int destRegIndex = bCore->threads[currTID].currInst->dst_index;
                int regAIndex = bCore->threads[currTID].currInst->src1_index;
                int Immediate = bCore->threads[currTID].currInst->src2_index_imm;
                int result = bCore->threads[currTID].regs->reg[regAIndex] - Immediate;
                bCore->threads[currTID].regs->reg[destRegIndex] = result;
                break;
            }
            case CMD_LOAD: {
                int destRegIndex = bCore->threads[currTID].currInst->dst_index;
                int regAIndex = bCore->threads[currTID].currInst->src1_index;
                int sourceA = bCore->threads[currTID].regs->reg[regAIndex];
                int sourceB;
                if (bCore->threads[currTID].currInst->isSrc2Imm) {
                    sourceB = bCore->threads[currTID].currInst->src2_index_imm;
                } else {
                    int regBIndex = bCore->threads[currTID].currInst->src2_index_imm;
                    sourceB = bCore->threads[currTID].regs->reg[regBIndex];
                }
                int result;
                SIM_MemDataRead(sourceA + sourceB, &result);
                bCore->threads[currTID].regs->reg[destRegIndex] = result;
                bCore->threads[currTID].idle = true;
                bCore->threads[currTID].timer = bCore->load_latency;
                contextSwitch = true;
                break;
            }
            case CMD_STORE: {
                int destRegIndex = bCore->threads[currTID].currInst->dst_index;
                int sourceB;
                if (bCore->threads[currTID].currInst->isSrc2Imm) {
                    sourceB = bCore->threads[currTID].currInst->src2_index_imm;
                } else {
                    int regBIndex = bCore->threads[currTID].currInst->src2_index_imm;
                    sourceB = bCore->threads[currTID].regs->reg[regBIndex];
                }
                int result = bCore->threads[currTID].regs->reg[destRegIndex] + sourceB;
                int regAIndex = bCore->threads[currTID].currInst->src1_index;
                int sourceA = bCore->threads[currTID].regs->reg[regAIndex];
                SIM_MemDataWrite(result, sourceA);
                bCore->threads[currTID].idle = true;
                bCore->threads[currTID].timer = bCore->store_latency;
                contextSwitch = true;
                break;

            }
            case CMD_HALT: {
                contextSwitch = true;
                bCore->threads[currTID].isHalted = true;
                bCore->threads[currTID].idle = true;
                bCore->threads[currTID].timer = 0;
                break;

            }


        }
        if (contextSwitch)
        {
            bool allIdle = true;
            bool allHalted = true;
            unsigned int minIdleTime = -1;
            int nextTID = 0;
            unsigned int lastActive = 0;
            for (unsigned int i = 0; i < bCore->th_number; i++) {
                if (bCore->threads[i].isHalted) bCore->threads[i].idle = true;
                allIdle = allIdle && bCore->threads[i].idle;
                allHalted = allHalted && bCore->threads[i].isHalted;
            }
            if (allHalted) return;

            if (allIdle)
            {
                for (int i = 0; i < bCore->th_number; i++) {
                    if (bCore->threads[i].isHalted) continue;
                    minIdleTime = (bCore->threads[i].timer < minIdleTime) ? bCore->threads[i].timer : minIdleTime;
                }
                for (int i = 0; i < bCore->th_number; i++) {
                    if (bCore->threads[i].isHalted) continue;

                    if (bCore->threads[i].timer == minIdleTime) {
                        if (bCore->threads[i].last_active > lastActive) {
                            lastActive = bCore->threads[i].last_active;
                            nextTID = i;
                        }
                    }
                }
                for (unsigned int i = 0; i < bCore->th_number; i++) {
                    if (bCore->threads[i].isHalted) continue;
                    bCore->threads[i].timer -= minIdleTime;
                    if (bCore->threads[i].timer == 0) {
                        bCore->threads[i].idle = false;
                    }

                }
                bCore->cycles += minIdleTime;
                if (nextTID != currTID) {
                    bCore->cycles += bCore->overhead;
                    currTID = nextTID;

                    for (unsigned int i = 0; i < bCore->th_number; i++) {
                        if (bCore->threads[i].isHalted) continue;

                        if (bCore->overhead >= bCore->threads[i].timer) {
                            bCore->threads[i].timer = 0;
                            bCore->threads[i].idle = false;
                        } else {
                            bCore->threads[i].timer -= bCore->overhead;
                        }
                    }
                }

            }
            else {
                int old_curr = currTID;
                bool skip = false;
                for (int i = currTID; i < bCore->th_number; i++) {
                    if (i == currTID) continue;
                    if (!bCore->threads[i].idle) {
                        currTID = i;
                        skip = true;
                        break;
                    }
                }
                if (!skip) {
                    for (int i = 0; i < currTID; i++) {
                        if (!bCore->threads[i].idle) {
                            currTID = i;
                            break;
                        }

                    }
                }
                if(currTID != old_curr)
                {
                    bCore->cycles += bCore->overhead;
                    for (unsigned int i = 0; i < bCore->th_number; i++) {
                        if (bCore->threads[i].isHalted) continue;
                        if (bCore->overhead >= bCore->threads[i].timer) {
                            bCore->threads[i].timer = 0;
                            bCore->threads[i].idle = false;
                        } else {
                            bCore->threads[i].timer -= bCore->overhead;
                        }
                    }
                }

            }
        }
    }
}

void CORE_FinegrainedMT() {
    fCore = CoreInit(false);


    int currTID = 0; //current thread ID
    int haltedCNT = 0;
    while (haltedCNT != fCore->th_number)
    {

        SIM_MemInstRead(fCore->threads[currTID].pc, fCore->threads[currTID].currInst, currTID);
        fCore->cycles++;
        fCore->instructions++;
        fCore->threads[currTID].last_active = fCore->cycles;
        fCore->threads[currTID].pc++;

        for (unsigned int i = 0; i < fCore->th_number; i++) {
            if (fCore->threads[i].isHalted) continue;
            if (1 >= fCore->threads[i].timer) {
                fCore->threads[i].timer = 0;
                fCore->threads[i].idle = false;
            } else {
                fCore->threads[i].timer -= 1;
            }
        }

        switch (fCore->threads[currTID].currInst->opcode) {
            case CMD_NOP:
                break;

            case CMD_ADD: {
                int destRegIndex = fCore->threads[currTID].currInst->dst_index;
                int regAIndex = fCore->threads[currTID].currInst->src1_index;
                int regBIndex = fCore->threads[currTID].currInst->src2_index_imm;
                int result =
                        fCore->threads[currTID].regs->reg[regAIndex] + fCore->threads[currTID].regs->reg[regBIndex];
                fCore->threads[currTID].regs->reg[destRegIndex] = result;
                break;
            }
            case CMD_SUB: {
                int destRegIndex = fCore->threads[currTID].currInst->dst_index;
                int regAIndex = fCore->threads[currTID].currInst->src1_index;
                int regBIndex = fCore->threads[currTID].currInst->src2_index_imm;
                int result =
                        fCore->threads[currTID].regs->reg[regAIndex] - fCore->threads[currTID].regs->reg[regBIndex];
                fCore->threads[currTID].regs->reg[destRegIndex] = result;
                break;
            }
            case CMD_ADDI: {
                int destRegIndex = fCore->threads[currTID].currInst->dst_index;
                int regAIndex = fCore->threads[currTID].currInst->src1_index;
                int Immediate = fCore->threads[currTID].currInst->src2_index_imm;
                int result = fCore->threads[currTID].regs->reg[regAIndex] + Immediate;
                fCore->threads[currTID].regs->reg[destRegIndex] = result;
                break;
            }
            case CMD_SUBI: {
                int destRegIndex = fCore->threads[currTID].currInst->dst_index;
                int regAIndex = fCore->threads[currTID].currInst->src1_index;
                int Immediate = fCore->threads[currTID].currInst->src2_index_imm;
                int result = fCore->threads[currTID].regs->reg[regAIndex] - Immediate;
                fCore->threads[currTID].regs->reg[destRegIndex] = result;
                break;
            }
            case CMD_LOAD: {
                int destRegIndex = fCore->threads[currTID].currInst->dst_index;
                int regAIndex = fCore->threads[currTID].currInst->src1_index;
                int sourceA = fCore->threads[currTID].regs->reg[regAIndex];
                int sourceB;
                if (fCore->threads[currTID].currInst->isSrc2Imm) {
                    sourceB = fCore->threads[currTID].currInst->src2_index_imm;
                } else {
                    int regBIndex = fCore->threads[currTID].currInst->src2_index_imm;
                    sourceB = fCore->threads[currTID].regs->reg[regBIndex];
                }
                int result;
                SIM_MemDataRead(sourceA + sourceB, &result);
                fCore->threads[currTID].regs->reg[destRegIndex] = result;
                fCore->threads[currTID].idle = true;
                fCore->threads[currTID].timer = fCore->load_latency;
                break;
            }
            case CMD_STORE: {
                int destRegIndex = fCore->threads[currTID].currInst->dst_index;
                int sourceB;
                if (fCore->threads[currTID].currInst->isSrc2Imm) {
                    sourceB = fCore->threads[currTID].currInst->src2_index_imm;
                } else {
                    int regBIndex = fCore->threads[currTID].currInst->src2_index_imm;
                    sourceB = fCore->threads[currTID].regs->reg[regBIndex];
                }
                int result = fCore->threads[currTID].regs->reg[destRegIndex] + sourceB;
                int regAIndex = fCore->threads[currTID].currInst->src1_index;
                int sourceA = fCore->threads[currTID].regs->reg[regAIndex];
                SIM_MemDataWrite(result, sourceA);
                fCore->threads[currTID].idle = true;
                fCore->threads[currTID].timer = fCore->store_latency;
                break;

            }
            case CMD_HALT: {
                fCore->threads[currTID].isHalted = true;
                fCore->threads[currTID].idle = true;
                fCore->threads[currTID].timer = 0;
                haltedCNT++;
                break;

            }


        }

        bool allIdle = true;
        bool allHalted = true;
        unsigned int minIdleTime = -1;
        int nextTID = 0;
        for (unsigned int i = 0; i < fCore->th_number; i++) {
            if (fCore->threads[i].isHalted) fCore->threads[i].idle = true;
            allIdle = allIdle && fCore->threads[i].idle;
            allHalted = allHalted && fCore->threads[i].isHalted;


        }
        if (allHalted) return;

        if (allIdle)
        {
            bool flag = false;

            for (int i = 0; i < fCore->th_number; i++) {
                if (fCore->threads[i].isHalted) continue;
                minIdleTime = (fCore->threads[i].timer < minIdleTime) ? fCore->threads[i].timer : minIdleTime;
            }
            for (int i = currTID+1; i < fCore->th_number; i++) {
                if (fCore->threads[i].isHalted) continue;

                if (fCore->threads[i].timer == minIdleTime) {

                    nextTID = i;
                    flag = true;
                    break;
                }

            }

            if(!flag)
            {
                for (int i = 0; i <= currTID; i++) {
                    if (fCore->threads[i].isHalted) continue;

                    if (fCore->threads[i].timer == minIdleTime) {
                        nextTID = i;
                        break;
                    }
                }
            }
            for (unsigned int i = 0; i < fCore->th_number; i++) {
                if (fCore->threads[i].isHalted) continue;
                fCore->threads[i].timer -= minIdleTime;
                if (fCore->threads[i].timer == 0) {
                    fCore->threads[i].idle = false;
                }

            }

            fCore->cycles += minIdleTime;
            if (nextTID != currTID) {
                currTID = nextTID;

            }

        }
        else {
            bool skip = false;
            for (int i = currTID+1; i < fCore->th_number; i++) {
                if(fCore->threads[i].isHalted) continue;
                if (!fCore->threads[i].idle) {
                    currTID = i;
                    skip = true;
                    break;
                }
            }
            if (!skip) {
                for (int i = 0; i < fCore->th_number; i++) {
                    if(fCore->threads[i].isHalted) continue;
                    if (!fCore->threads[i].idle) {
                        currTID = i;
                        break;
                    }

                }
            }

        }



    }
}

double CORE_BlockedMT_CPI(){
    double CPI = (double)bCore->cycles / bCore->instructions;
    for (int i = 0; i < bCore->th_number ; ++i)
    {
        free(bCore->threads[i].regs);
    }
    free(bCore->threads);
    free(bCore);

    return CPI;
}

double CORE_FinegrainedMT_CPI(){
    double CPI = (double)fCore->cycles / fCore->instructions;
    for (int i = 0; i < fCore->th_number ; ++i)
    {
        free(fCore->threads[i].regs);
    }
    free(fCore->threads);
    free(fCore);

    return CPI;
}

void CORE_BlockedMT_CTX(tcontext* context, int threadid) {
    if(!context)
    {
        return;
    }
    if(threadid >= bCore->th_number) return;
    for(int i = 0 ; i < REGS_COUNT ; i++)
    {
        context[threadid].reg[i] = bCore->threads[threadid].regs->reg[i];
    }
}

void CORE_FinegrainedMT_CTX(tcontext* context, int threadid) {
    if(!context)
    {
        return;
    }
    if(threadid >= fCore->th_number) return;
    for(int i = 0 ; i < REGS_COUNT ; i++)
    {
        context[threadid].reg[i] = fCore->threads[threadid].regs->reg[i];
    }
}
