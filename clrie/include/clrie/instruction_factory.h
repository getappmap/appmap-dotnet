#pragma once

#include <cor.h>
#include <no_sal.h>
#include <InstrumentationEngine.h>
#include "com/ptr.h"

namespace clrie {
    struct instruction_factory
    {
        instruction_factory(com::ptr<IInstructionFactory> &&factory) noexcept : ptr(std::move(factory)) {}

        using instruction = com::ptr<IInstruction>;
        using instruction_sequence = std::vector<instruction>;

        // Create an instance of an instruction that takes no operands
        com::ptr<IInstruction> create_instruction(enum ILOrdinalOpcode opcode) {
            com::ptr<IInstruction> result;
            com::hresult::check(ptr->CreateInstruction(opcode, &result));
            return result;
        }

        // Create an instance of an instruction that takes a byte operand
        com::ptr<IInstruction> create_byte_operand_instruction(enum ILOrdinalOpcode opcode, BYTE operand) {
            com::ptr<IInstruction> result;
            com::hresult::check(ptr->CreateByteOperandInstruction(opcode, operand, &result));
            return result;
        }

        // Create an instance of an instruction that takes a USHORT operand
        com::ptr<IInstruction> create_u_short_operand_instruction(enum ILOrdinalOpcode opcode, USHORT operand) {
            com::ptr<IInstruction> result;
            com::hresult::check(ptr->CreateUShortOperandInstruction(opcode, operand, &result));
            return result;
        }

        // Create an instance of an instruction that takes a ULONG operand
        com::ptr<IInstruction> create_int_operand_instruction(enum ILOrdinalOpcode opcode, INT32 operand) {
            com::ptr<IInstruction> result;
            com::hresult::check(ptr->CreateIntOperandInstruction(opcode, operand, &result));
            return result;
        }

        // Create an instance of an instruction that takes a 64 bit operand. The name "Long" in this case comes from the msil definition
        // of a 64 bit value
        com::ptr<IInstruction> create_long_operand_instruction(enum ILOrdinalOpcode opcode, INT64 operand) {
            com::ptr<IInstruction> result;
            com::hresult::check(ptr->CreateLongOperandInstruction(opcode, operand, &result));
            return result;
        }

        template <typename T>
        instruction load_constant(T value) {
            IInstruction *result;
            if constexpr (sizeof(T) == sizeof(uint64_t)) {
                com::hresult::check(ptr->CreateLongOperandInstruction(Cee_Ldc_I8, reinterpret_cast<uint64_t>(value), &result));
            } else if constexpr (sizeof(T) == sizeof(uint32_t)) {
                com::hresult::check(ptr->CreateIntOperandInstruction(Cee_Ldc_I4, reinterpret_cast<uint32_t>(value), &result));
            } else {
                static_assert(sizeof(T) < 0, "loading constant of T not implemented");
            }
            return result;
        }

        template <typename... Args>
        instruction_sequence load_constants(Args... args) {
            return { load_constant(args)... };
        }

        // Create an instance of an instruction that takes a float operand
        com::ptr<IInstruction> create_float_operand_instruction(enum ILOrdinalOpcode opcode, float operand) {
            com::ptr<IInstruction> result;
            com::hresult::check(ptr->CreateFloatOperandInstruction(opcode, operand, &result));
            return result;
        }

        // Create an instance of an instruction that takes a double operand
        com::ptr<IInstruction> create_double_operand_instruction(enum ILOrdinalOpcode opcode, double operand) {
            com::ptr<IInstruction> result;
            com::hresult::check(ptr->CreateDoubleOperandInstruction(opcode, operand, &result));
            return result;
        }

        // Create an instance of an instruction that takes a metadata token operand
        com::ptr<IInstruction> create_token_operand_instruction(enum ILOrdinalOpcode opcode, mdToken operand) {
            com::ptr<IInstruction> result;
            com::hresult::check(ptr->CreateTokenOperandInstruction(opcode, operand, &result));
            return result;
        }

        // Create a branch instruction
        com::ptr<IInstruction> create_branch_instruction(enum ILOrdinalOpcode opcode, com::ptr<IInstruction> branch_target) {
            com::ptr<IInstruction> result;
            com::hresult::check(ptr->CreateBranchInstruction(opcode, branch_target, &result));
            return result;
        }

        // Create a switch instruction
        com::ptr<IInstruction> create_switch_instruction(enum ILOrdinalOpcode opcode, const std::vector<IInstruction *> &targets) {
            com::ptr<IInstruction> result;
            com::hresult::check(ptr->CreateSwitchInstruction(opcode, targets.size(), const_cast<IInstruction **>(targets.data()), &result));
            return result;
        }


        // Helpers that adjust the opcode to match the operand size

        // Creates an operand instruction of type Cee_Ldc_I4, or Cee_Ldc_I4_S, Cee_Ldc_I4_0, Cee_Ldc_I4_1, Cee_Ldc_I4_2...
        com::ptr<IInstruction> create_load_const_instruction(int value) {
            IInstruction *result;
            com::hresult::check(ptr->CreateLoadConstInstruction(value, &result));
            return result;
        }

        // Creates an operand instruction of type Cee_Ldloc, or (Cee_Ldloc_S, (Cee_Ldloc_0, Cee_Ldloc_1, Cee_Ldloc_1...
        com::ptr<IInstruction> create_load_local_instruction(USHORT index) {
            com::ptr<IInstruction> result;
            com::hresult::check(ptr->CreateLoadLocalInstruction(index, &result));
            return result;
        }

        // Creates an operand instruction of type Cee_Ldloca, or Cee_Ldloca_S
        com::ptr<IInstruction> create_load_local_address_instruction(USHORT index) {
            com::ptr<IInstruction> result;
            com::hresult::check(ptr->CreateLoadLocalAddressInstruction(index, &result));
            return result;
        }

        // Creates an operand instruction of type Cee_Stloc, or Cee_Stloc_S, Cee_Stloc_0, Cee_Stloc_1, Cee_Stloc_2...
        com::ptr<IInstruction> create_store_local_instruction(USHORT index) {
            com::ptr<IInstruction> result;
            com::hresult::check(ptr->CreateStoreLocalInstruction(index, &result));
            return result;
        }

        // Creates an operand instruction of type Cee_Ldarg, or Cee_Ldarg_S, Cee_Ldarg_0, Cee_Ldarg_1, Cee_Ldarg_2...
        com::ptr<IInstruction> create_load_arg_instruction(USHORT index) const {
            com::ptr<IInstruction> result;
            com::hresult::check(ptr->CreateLoadArgInstruction(index, &result));
            return result;
        }

        // Creates an operand instruction of type Cee_Ldarga or Cee_Ldarga_S
        com::ptr<IInstruction> create_load_arg_address_instruction(USHORT index) {
            com::ptr<IInstruction> result;
            com::hresult::check(ptr->CreateLoadArgAddressInstruction(index, &result));
            return result;
        }

        // Given a byte stream that points to MSIL, create new graph of IInstructions that represents
        // that stream.
        //
        // 1) Each instruction in this stream will be marked as "New" and will not have the original
        //    next and original previous fields set.
        //
        // 2) This graph will not have a method info set and cannot create exception sections or handlers.
        com::ptr<IInstructionGraph> decode_instruction_byte_stream(const std::vector<uint8_t> &stream) {
            com::ptr<IInstructionGraph> result;
            com::hresult::check(ptr->DecodeInstructionByteStream(stream.size(), stream.data(), &result));
            return result;
        }

    private:
        mutable com::ptr<IInstructionFactory> ptr;
    };    
}
