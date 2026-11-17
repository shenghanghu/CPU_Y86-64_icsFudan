#include "cpu_common.h"

/* 每个阶段通过 CPU 结构体中的暂存值传递数据。请补全以下六个阶段。 */
void fetch(CPU *cpu)
{
    //command
    uint64_t byte0 = read_mem(cpu, cpu->pc, 1);
    if (cpu->stat != AOK) return;
    cpu->icode = (int)(byte0 >> 4);
    cpu->ifun = (int)(byte0 & 15);

    int max_ifun = 0;
    //检查icode和ifun是否有效
    if(cpu->icode == I_ALU){
        max_ifun = 3;
    }
    else if(cpu->icode == I_JMP || cpu->icode == I_RRMOVQ){
        max_ifun = 6;
    }

    if(cpu->icode > I_POPQ || cpu->ifun > max_ifun){
        cpu->stat = INS;
        return;
    }

    if(cpu->icode == I_HALT){
        cpu->stat = HLT;
        return;
    }
    cpu->valP = cpu->pc + 1;
    cpu->rA = RNONE;
    cpu->rB = RNONE;

    //是否需要reg参数
    int need_reg = 
        cpu->icode == I_RRMOVQ ||
        cpu->icode == I_IRMOVQ ||
        cpu->icode == I_RMMOVQ ||
        cpu->icode == I_MRMOVQ ||
        cpu->icode == I_ALU    ||
        cpu->icode == I_PUSHQ  ||
        cpu->icode == I_POPQ;

    if(need_reg) {
        uint64_t reg_byte = read_mem(cpu, cpu->valP, 1);
        if (cpu->stat != AOK) return;
        
        cpu->rA = (int)(reg_byte >> 4);
        cpu->rB = (int)(reg_byte & 15);
        //reg是否有效
        int valid_reg;

        if(cpu->icode == I_IRMOVQ){
            valid_reg = cpu->rA == RNONE && cpu->rB >= RAX && cpu->rB <= R14;
        }
        else if(cpu->icode == I_PUSHQ || cpu->icode == I_POPQ){
            valid_reg = cpu->rA >= RAX && cpu->rA <= R14 && cpu->rB == RNONE;
        }
        else{
            valid_reg = cpu->rA >= RAX && cpu->rA <= R14 && cpu->rB >= RAX && cpu->rB <= R14;
        }

        if(!valid_reg){
            cpu->stat = INS;
            return;
        }
        cpu->valP += 1;
    }

    int need_valC = 
        cpu->icode == I_IRMOVQ ||
        cpu->icode == I_RMMOVQ ||
        cpu->icode == I_MRMOVQ ||
        cpu->icode == I_JMP    ||
        cpu->icode == I_CALL;
    cpu->valC = 0;
    if(need_valC){
        cpu->valC = read_mem(cpu, cpu->valP, 8);
        if (cpu->stat != AOK) return;
        cpu->valP += 8;
    }
    
    
}

void decode(CPU *cpu)
{
    cpu->valA = 0;
    cpu->valB = 0;

    if (cpu->icode == I_RRMOVQ ||
        cpu->icode == I_RMMOVQ ||
        cpu->icode == I_ALU    ||
        cpu->icode == I_PUSHQ) {
        cpu->valA = cpu->reg[cpu->rA];
    } else if (cpu->icode == I_POPQ ||
            cpu->icode == I_RET) {
        cpu->valA = cpu->reg[RSP];
    }

    if (cpu->icode == I_RMMOVQ ||
        cpu->icode == I_MRMOVQ ||
        cpu->icode == I_ALU) {
        cpu->valB = cpu->reg[cpu->rB];
    } else if (cpu->icode == I_PUSHQ ||
            cpu->icode == I_POPQ  ||
            cpu->icode == I_CALL  ||
            cpu->icode == I_RET) {
        cpu->valB = cpu->reg[RSP];
    }
}

void execute(CPU *cpu)
{
    cpu->valE = 0;
    cpu->cnd = 0;

    if (cpu->icode == I_RRMOVQ) {
        cpu->valE = cpu->valA;
        cpu->cnd = check_cond(cpu, cpu->ifun);
    } 
    else if (cpu->icode == I_IRMOVQ) {
        cpu->valE = cpu->valC;
    } 
    else if (cpu->icode == I_RMMOVQ ||
               cpu->icode == I_MRMOVQ) {
        cpu->valE = cpu->valB + cpu->valC;
    } 
    else if (cpu->icode == I_PUSHQ ||
               cpu->icode == I_CALL) {
        cpu->valE = cpu->valB - 8;
    } 
    else if (cpu->icode == I_POPQ ||
               cpu->icode == I_RET) {
        cpu->valE = cpu->valB + 8;
    } 
    else if (cpu->icode == I_JMP) {
        cpu->cnd = check_cond(cpu, cpu->ifun);
    }
    else if (cpu->icode == I_ALU){
        switch (cpu->ifun) {
        case 0: // addq
            cpu->valE = cpu->valB + cpu->valA;
            break;
        case 1: // subq
            cpu->valE = cpu->valB - cpu->valA; 
            break;
        case 2: // andq
            cpu->valE = cpu->valB & cpu->valA;
            break;
        case 3: // xorq
            cpu->valE = cpu->valB ^ cpu->valA;
            break;
        default:
            cpu->stat = INS;
            return;
        }
        // 设置条件码
        int signA = (int)(cpu->valA >> 63);
        int signB = (int)(cpu->valB >> 63);
        int signE = (int)(cpu->valE >> 63);

        cpu->zf = (cpu->valE == 0);
        cpu->sf = signE;
        cpu->of = 0;

        if (cpu->ifun == 0) {
            // 加法：同号相加，结果变号
            cpu->of = (signA == signB) && (signE != signB);
        } else if (cpu->ifun == 1) {
            // 减法 B-A：操作数异号，结果与 B 异号
            cpu->of = (signA != signB) && (signE != signB);
        }
    }

    
}

void memory_stage(CPU *cpu)
{
    cpu->valM = 0;
    if (cpu->stat != AOK) return;

    if (cpu->icode == I_RMMOVQ ||
        cpu->icode == I_PUSHQ) {
        write_mem(cpu, cpu->valE, 8, cpu->valA);
    } else if (cpu->icode == I_CALL) {
        write_mem(cpu, cpu->valE, 8, cpu->valP);
    } else if (cpu->icode == I_MRMOVQ) {
        cpu->valM = read_mem(cpu, cpu->valE, 8);
    } else if (cpu->icode == I_POPQ ||
               cpu->icode == I_RET) {
        cpu->valM = read_mem(cpu, cpu->valA, 8);
    }
}

void write_back(CPU *cpu)
{
    int dstE = RNONE;
    int dstM = RNONE;

    if (cpu->icode == I_RRMOVQ) {
        if (cpu->cnd) {
            dstE = cpu->rB;
        }
    } else if (cpu->icode == I_IRMOVQ ||
               cpu->icode == I_ALU) {
        dstE = cpu->rB;
    } else if (cpu->icode == I_PUSHQ ||
               cpu->icode == I_POPQ  ||
               cpu->icode == I_CALL  ||
               cpu->icode == I_RET) {
        dstE = RSP;
    }

    if (cpu->icode == I_MRMOVQ ||
        cpu->icode == I_POPQ) {
        dstM = cpu->rA;
    }

    if (dstE != RNONE) {
        cpu->reg[dstE] = cpu->valE;
    }
    if (dstM != RNONE) {
        cpu->reg[dstM] = cpu->valM;
    }
}

void update_pc(CPU *cpu)
{
    if (cpu->icode == I_CALL) {
        cpu->pc = cpu->valC;
    } else if (cpu->icode == I_JMP) {
        if (cpu->cnd) {
            cpu->pc = cpu->valC;
        } else {
            cpu->pc = cpu->valP;
        }
    } else if (cpu->icode == I_RET) {
        cpu->pc = cpu->valM;
    } else {
        cpu->pc = cpu->valP;
    }
}
