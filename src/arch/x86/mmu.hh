/*
 * Copyright (c) 2020 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __ARCH_X86_MMU_HH__
#define __ARCH_X86_MMU_HH__

#include "arch/generic/mmu.hh"
#include "arch/x86/page_size.hh"
#include "arch/x86/tlb.hh"

// Shiming: to use functions in walker
#include "arch/x86/pagetable_walker.hh"

// Shiming: add pwc
#include "arch/x86/translation_cache.hh"
#include "params/X86MMU.hh"

namespace gem5
{

namespace X86ISA {

class MMU : public BaseMMU
{
    // Shiming: pwc related members
  public:
    PageStructureCache* pwc;
  public:
    // Shiming: construct pwc
    MMU(const X86MMUParams &p)
      : BaseMMU(p)
    {
      enablePwc = p.enable_pwc;
      if (enablePwc) {
        pwc = new PageStructureCache(name(), p.pwc_pml4_size,
            p.pwc_pdp_size, p.pwc_pde_size);

        static_cast<TLB*>(dtb)->getWalker()->setEnablePwc();
        static_cast<TLB*>(dtb)->getWalker()->setPwc(pwc);
        static_cast<TLB*>(itb)->getWalker()->setEnablePwc();
        static_cast<TLB*>(itb)->getWalker()->setPwc(pwc);
      } else {
        pwc = nullptr;
      }
    }

    // Shiming: delete pwc
    ~MMU() {
      if (enablePwc) {
        delete pwc;
      }
    }

    // Shiming:
  public:
    void
    flushPwc() override {
      if (enablePwc) {
        assert(pwc);
        pwc->flush();
      }
    }

    void
    flushNonGlobal()
    {
        static_cast<TLB*>(itb)->flushNonGlobal();
        static_cast<TLB*>(dtb)->flushNonGlobal();
        // Shiming: always flushall according to document
        if (enablePwc) { flushPwc(); }
    }

    Walker*
    getDataWalker()
    {
        return static_cast<TLB*>(dtb)->getWalker();
    }

    TranslationGenPtr
    translateFunctional(Addr start, Addr size, ThreadContext *tc,
            Mode mode, Request::Flags flags) override
    {
        return TranslationGenPtr(new MMUTranslationGen(
                PageBytes, start, size, tc, this, mode, flags));
    }
};

} // namespace X86ISA
} // namespace gem5

#endif // __ARCH_X86_MMU_HH__
