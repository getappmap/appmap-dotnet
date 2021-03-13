#pragma once

#include <InstrumentationEngine.h>
#include "com/ptr.h"
#include "cor.h"

namespace clrie {
    struct instruction_graph : public com::ptr<IInstructionGraph> {
        instruction_graph(com::ptr<IInstructionGraph> &&graph) noexcept : ptr(std::move(graph)) {}

        com::ptr<IMethodInfo> method_info() {
            return get(&interface_type::GetMethodInfo);
        }

        // Get the current first and last instructions reflecting changes that instrumentation methods have made
        com::ptr<IInstruction> first_instruction() {
            return get(&interface_type::GetFirstInstruction);
        }
        com::ptr<IInstruction> last_instruction() {
            return get(&interface_type::GetLastInstruction);
        }

        com::ptr<IInstruction> original_first_instruction() {
            return get(&interface_type::GetOriginalFirstInstruction);
        }
        com::ptr<IInstruction> original_last_instruction() {
            return get(&interface_type::GetOriginalLastInstruction);
        }

        com::ptr<IInstruction> uninstrumented_first_instruction() {
            return get(&interface_type::GetUninstrumentedFirstInstruction);
        }
        com::ptr<IInstruction> uninstrumented_last_instruction() {
            return get(&interface_type::GetUninstrumentedLastInstruction);
        }

        com::ptr<IInstruction> instruction_at_offset(unsigned int offset);
        com::ptr<IInstruction> instruction_at_original_offset(unsigned int offset);

        com::ptr<IInstruction> instruction_at_uninstrumented_offset(unsigned int dw_offset);

        // Insert an instruction before another instruction. jmp offsets that point to the original instruction
        // are not updated to reflect this change
        void insert_before(com::ptr<IInstruction> instruction_orig, com::ptr<IInstruction> instruction_new)
        {
            com::hresult::check(ptr_->InsertBefore(instruction_orig, instruction_new));
        }

        template <typename Container>
        void insert_before(com::ptr<IInstruction> pos, const Container &&instructions)
        {
            for (auto ins : instructions)
                insert_before(pos, ins);
        }

        // Insert an instruction after another instruction. jmp offsets that point to the next instruction after
        // the other instruction are not updated to reflect this change
        void insert_after(com::ptr<IInstruction> instruction_orig, com::ptr<IInstruction> instruction_new);

        // Insert an instruction before another instruction AND update jmp targets and exception ranges that used
        // to point to the old instruction to point to the new instruction.
        void insert_before_and_retarget_offsets(com::ptr<IInstruction> instruction_orig, com::ptr<IInstruction> instruction_new);

        // Replace an instruction with another instruction. The old instruction continues to live in the original graph but is marked replaced
        void replace(com::ptr<IInstruction> instruction_orig, com::ptr<IInstruction> *p_instruction_new);

        // Remove an instruction. The old instruction continues to live in the original graph but is marked deleted
        void remove(com::ptr<IInstruction> *p_instruction_orig);

        // Remove all instructions from the current graph. The original instructions are still accessible from the original first and original last
        // methods. These are all marked deleted  and their original next fields are still set.
        void remove_all();

        // Removes all existing instructions from the graph and replaces them with a complete method body. The original instructions are preserved in a disjoint
        // graph. Note that this changes the behavior of API's such as GetInstructionAtOriginalOffset; these will now retrieve the baseline instruction instead.
        // This can only be called during the BeforeInstrumentMethod phase of the instrumentation and can only be called once
        HRESULT CreateBaseline();

        // true if CreateBaseline has previously been called.
        bool has_baseline_been_set() {
            return get(&interface_type::HasBaselineBeenSet);
        }

        void expand_branches();
    };
}
