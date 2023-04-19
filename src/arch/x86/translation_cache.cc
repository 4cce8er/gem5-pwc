/**
 * 2023 Feb
 * Shiming Li
 * shiming.li@it.uu.se
 *
 * Uppsala Architecture Research Team (UART)
 * Uppsala University
 *
 * This is an implementation of a translation cache as described in
 *  http://kib.kiev.ua/x86docs/Intel/WhitePapers/317080-002.pdf. Code is
 *  based on tlb.hh and tlb.cc.
 *
 * It should be fine to not use Serializable for checkpointing since these
 *  caches does not hold modified entries (i.e. they are write-through).
 */

#include "arch/x86/translation_cache.hh"

#include <list>
#include <vector>

#include "arch/x86/pagetable.hh" // Shiming: To use PageTableEntry
#include "arch/x86/pagetable_walker.hh" // Shiming: To use State
#include "base/bitfield.hh" // Shiming: To use mbit
#include "base/trie.hh"

namespace gem5
{
namespace X86ISA
{
    void BaseTranslationCache::evictLRU() {
        unsigned lru = 0;
        for (unsigned i = 1; i < size; i++) {
            if (tc[i].lruSeq < tc[lru].lruSeq) {
                lru = i;
            }
        }

        assert(tc[lru].trieHandle);
        trie.remove(tc[lru].trieHandle);
        tc[lru].trieHandle = NULL;
        freeList.push_back(&tc[lru]);
        stats.evict++;
    }

    BaseTranslationCache::BaseTranslationCache(std::string _name,
            uint32_t _size, unsigned _idx_mask_bits_h,
            unsigned _idx_mask_bits_l)
            : size(_size), myName(_name), idxMaskBitsH(_idx_mask_bits_h),
                idxMaskBitsL(_idx_mask_bits_l), tc(_size) {
        addrMask = (~(Addr)0 >> idxMaskBitsH) & (~(Addr)0 << idxMaskBitsL);
        for (int x = 0; x < size; x++) {
            tc[x].trieHandle = NULL;
            freeList.push_back(&tc[x]);
        }
        stats.flush.name(name() + "flush");
        stats.insert.name(name() + "insert");
        stats.evict.name(name() + "evict");
        stats.hit.name(name() + "hit");
        stats.miss.name(name() + "miss");
    }

    TranslationCacheEntry* BaseTranslationCache::insert(Addr vpn,
                const PageTableEntry &ptentry, LegacyAcc la) {
        Addr idx = maskVpn(legacyMask(vpn, la));
        // If somebody beat us to it, just use that existing entry.
        TranslationCacheEntry *newEntry = trie.lookup(idx);
        if (newEntry) {
            assert(newEntry->index == idx);
            assert(newEntry->nextStepEntry == ptentry);
            return newEntry;
        }

        if (freeList.empty()) {
            evictLRU();
        }
        newEntry = freeList.front();
        freeList.pop_front();

        newEntry->nextStepEntry = ptentry;
        newEntry->lruSeq = nextSeq();
        newEntry->index = idx;
        newEntry->trieHandle =
            trie.insert(idx,
                TranslationCacheEntryTrie::MaxBits - getIdxMaskBitsL(),
                newEntry);
        stats.insert++;
        return newEntry;
    }

    TranslationCacheEntry* BaseTranslationCache::lookup(Addr va,
            LegacyAcc la, bool update_lru) {
        TranslationCacheEntry* entry
            = trie.lookup(maskVpn(legacyMask(va, la)));
        if (entry) {
            stats.hit++;
            if (update_lru) {
                entry->lruSeq = nextSeq();
            }
        } else {
            stats.miss++;
        }
        return entry;
    }

    void BaseTranslationCache::flush() {
        /**
         * On writes to CR3 (TLB flush non-global) or CR4 (TLB flush
         * all), TC should always flush all.
         */
        for (unsigned i = 0; i < size; i++) {
            if (tc[i].trieHandle) {
                trie.remove(tc[i].trieHandle);
                tc[i].trieHandle = NULL;
                freeList.push_back(&tc[i]);
            }
        }
        stats.flush++;
    }

    // Child classes
    Addr PageStructureCache::PML4Cache::legacyMask(Addr vpn, LegacyAcc la) {
        switch (la) {
            case NONE: {
                return vpn;
            }

            case LEGACY_32b_PAE:
            case LEGACY_32b_NO_PAE:
            default: {
                panic("PML4 cache should not be used in legacy mode");
            }
        }
    }

    Addr PageStructureCache::PDPCache::legacyMask(Addr vpn, LegacyAcc la) {
        switch (la) {
            case NONE: {
                return vpn;
            }
            case LEGACY_32b_PAE: {
                // "TLBs, Paging-Structure Caches, and Their Invalidation"
                //  8.1 and Table 1
                return mbits(vpn, 31, 30);
            }
            case LEGACY_32b_NO_PAE:
            default: {
                panic("PDP cache should not be used in this mode: %d", la);
            }
        }
    }

    Addr PageStructureCache::PDECache::legacyMask(Addr vpn, LegacyAcc la) {
        switch (la) {
            case NONE: {
                return vpn;
            }
            case LEGACY_32b_PAE: {
                // "TLBs, Paging-Structure Caches, and Their Invalidation"
                //  8.1 and Table 1
                return mbits(vpn, 31, 21);
            }
            case LEGACY_32b_NO_PAE: {
                // "TLBs, Paging-Structure Caches, and Their Invalidation"
                //  8.2 and Table 1
                return mbits(vpn, 31, 22);
            }
            default: {
                panic("PDE cache meets unrecognized legacy mode %d", la);
            }
        }
    }

    void PageStructureCache::flush() {
        pml4Cache.flush();
        pdpCache.flush();
        pdeCache.flush();
    }
} // namespace X86ISA
} // namespace gem5
