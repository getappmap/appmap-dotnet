#pragma once

#include <InstrumentationEngine.h>
#include "com/ptr.h"
#include "cor.h"

namespace clrie {
    struct instruction_graph : public com::ptr<IInstructionGraph> {
        // not strictly an up to spec iterator, but just enough for our purposes
        struct iterator: com::ptr<IInstruction>
        {
            using ptr::ptr;
            iterator(ptr other): ptr(std::move(other)) {}

            iterator &operator++() {
                iterator next;
                if (ptr_->GetNextInstruction(&next) == S_OK) {
                    return (*this) = next;
                } else {
                    return (*this) = nullptr;
                }
            }
        };

        instruction_graph(com::ptr<IInstructionGraph> &&graph) noexcept : ptr(std::move(graph)) {}

        com::ptr<IMethodInfo> method_info() {
            return get(&interface_type::GetMethodInfo);
        }

        // Get the current first and last instructions reflecting changes that instrumentation methods have made
        iterator first_instruction() {
            return get(&interface_type::GetFirstInstruction);
        }
        iterator last_instruction() {
            return get(&interface_type::GetLastInstruction);
        }

        iterator original_first_instruction() {
            return get(&interface_type::GetOriginalFirstInstruction);
        }
        iterator original_last_instruction() {
            return get(&interface_type::GetOriginalLastInstruction);
        }

        iterator uninstrumented_first_instruction() {
            return get(&interface_type::GetUninstrumentedFirstInstruction);
        }
        iterator uninstrumented_last_instruction() {
            return get(&interface_type::GetUninstrumentedLastInstruction);
        }

        iterator instruction_at_offset(unsigned int offset);
        iterator instruction_at_original_offset(unsigned int offset);

        iterator instruction_at_uninstrumented_offset(unsigned int dw_offset);

        // Insert an instruction before another instruction. jmp offsets that point to the original instruction
        // are not updated to reflect this change
        void insert_before(iterator instruction_orig, com::ptr<IInstruction> instruction_new)
        {
            com::hresult::check(ptr_->InsertBefore(instruction_orig, instruction_new));
        }

        template <typename Container>
        void insert_before(iterator pos, const Container &instructions)
        {
            for (auto ins : instructions)
                insert_before(pos, ins);
        }

        // Insert an instruction after another instruction. jmp offsets that point to the next instruction after
        // the other instruction are not updated to reflect this change
        void insert_after(iterator instruction_orig, com::ptr<IInstruction> instruction_new) {
            com::hresult::check(ptr_->InsertAfter(instruction_orig, instruction_new));
        }

        // Insert an instruction before another instruction AND update jmp targets and exception ranges that used
        // to point to the old instruction to point to the new instruction.
        void insert_before_and_retarget_offsets(iterator instruction_orig, com::ptr<IInstruction> instruction_new)
        {
            com::hresult::check(ptr_->InsertBeforeAndRetargetOffsets(instruction_orig, instruction_new));
        }

        template <typename Container>
        void insert_before_and_retarget_offsets(iterator pos, const Container &instructions)
        {
            bool retargeted = false;
            for (auto ins : instructions) {
                if (retargeted)
                    insert_before(pos, ins);
                else {
                    insert_before_and_retarget_offsets(pos, ins);
                    retargeted = true;
                }
            }
        }

        // Replace an instruction with another instruction. The old instruction continues to live in the original graph but is marked replaced
        void replace(iterator instruction_orig, com::ptr<IInstruction> *p_instruction_new);

        // Remove an instruction. The old instruction continues to live in the original graph but is marked deleted
        void remove(iterator *p_instruction_orig);

        // Remove all instructions from the current graph. The original instructions are still accessible from the original first and original last
        // methods. These are all marked deleted  and their original next fields are still set.
        void remove_all() {
            com::hresult::check(ptr_->RemoveAll());
        }

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
