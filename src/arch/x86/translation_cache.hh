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

#include <list>
#include <vector>

#include "arch/x86/pagetable.hh" // To use PageTableEntry

//#include "arch/x86/pagetable_walker.hh"
#include "base/trie.hh"

#ifndef __ARCH_X86_TRANSLATION_CACHE_HH__
#define __ARCH_X86_TRANSLATION_CACHE_HH__

namespace gem5
{

namespace X86ISA
{
    // Declare here first, and define later. Need it for declaring Trie
    struct TranslationCacheEntry;
} // namespace X86ISA

typedef Trie<Addr, X86ISA::TranslationCacheEntry> TranslationCacheEntryTrie;

namespace X86ISA
{
    struct TranslationCacheEntry
    {
        Addr index;
        PageTableEntry nextStepEntry;
        uint64_t lruSeq;
        TranslationCacheEntryTrie::Handle trieHandle;
    };

    class BaseTranslationCache
    {
        public:
            enum LegacyAcc
            {
                NONE = 0,
                LEGACY_32b_PAE,
                LEGACY_32b_NO_PAE
            };
        private:
            uint32_t size;
            uint64_t lruSeq;

        protected:
            unsigned idxMaskBitsH = 0;
            unsigned idxMaskBitsL = 0; // trie.insert() needs this type
            uint64_t addrMask = 0;

            std::list<TranslationCacheEntry *> freeList;
            std::vector<TranslationCacheEntry> tc;

            TranslationCacheEntryTrie trie;

        private:
            uint64_t nextSeq() { return ++lruSeq; }

            void evictLRU();

            inline unsigned getIdxMaskBitsL() { return idxMaskBitsL; }
            inline Addr maskVpn(Addr vpn) { return addrMask & vpn; }

            virtual Addr legacyMask(Addr vpn, LegacyAcc la) {
                panic("no impl");
            }

        protected:
            BaseTranslationCache(uint32_t _size,
                unsigned _idx_mask_bits_h, unsigned _idx_mask_bits_l);
            virtual ~BaseTranslationCache() = default;

        public:
            TranslationCacheEntry* insert(Addr vpn,
                    const PageTableEntry &ptentry,
                    LegacyAcc la=LegacyAcc::NONE);
            TranslationCacheEntry* lookup(Addr va,
                    LegacyAcc la=LegacyAcc::NONE, bool update_lru=true);
        public:
            void flush();
            //void demapPage(Addr va, uint64_t asn);
    };

    class PML4Cache: public BaseTranslationCache
    {
        public:
            PML4Cache(uint32_t _size) : BaseTranslationCache(_size, 12, 39) {}
            Addr legacyMask(Addr vpn, LegacyAcc la) override;
            ~PML4Cache() {}
    };

    class PDPCache: public BaseTranslationCache
    {
        public:
            PDPCache(uint32_t _size) : BaseTranslationCache(_size, 12, 30) {}
            Addr legacyMask(Addr vpn, LegacyAcc la) override;
            ~PDPCache() {}
    };

    class PDECache: public BaseTranslationCache
    {
        public:
            PDECache(uint32_t _size) : BaseTranslationCache(_size, 12, 21) {}
            Addr legacyMask(Addr vpn, LegacyAcc la) override;
            ~PDECache() {}
    };

    struct PageWalkCachePtr
    {
        PML4Cache* pml4cache = nullptr;
        PDPCache* pdpcache = nullptr;
        PDECache* pdecache = nullptr;
    };

} // namespace X86ISA
} // namespace gem5

#endif // __ARCH_X86_TRANSLATION_CACHE_HH__
