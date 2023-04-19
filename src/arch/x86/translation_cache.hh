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

#include "arch/x86/pagetable.hh" // Shiming: To use PageTableEntry
#include "arch/x86/pagetable_walker.hh" // Shiming: To use State
#include "base/trie.hh"
#include "sim/stats.hh"

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
    enum PageWalkState : short; // Defined in pagetable_walker.hh

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
            std::string myName;

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
            BaseTranslationCache(std::string _name, uint32_t _size,
                unsigned _idx_mask_bits_h, unsigned _idx_mask_bits_l);
            virtual ~BaseTranslationCache() = default;

            struct TranslationCacheStats
            {
                statistics::Scalar flush;
                statistics::Scalar insert;
                statistics::Scalar evict;
                statistics::Scalar hit;
                statistics::Scalar miss;
            } stats;
        public:
            TranslationCacheEntry* insert(Addr vpn,
                    const PageTableEntry &ptentry,
                    LegacyAcc la=LegacyAcc::NONE);
            TranslationCacheEntry* lookup(Addr va,
                    LegacyAcc la=LegacyAcc::NONE, bool update_lru=true);
        public:
            void flush();
            std::string name() const { return myName; }
    };

    /**
     * This mimics an Intel page structure cache
     */
    class PageStructureCache
    {
        private:
            /** Create 3 split translation caches for the first 3 level walk */
            class PML4Cache: public BaseTranslationCache
            {
                public:
                    PML4Cache(std::string _name, uint32_t _size)
                        : BaseTranslationCache(_name, _size, 12, 39) {}
                    Addr legacyMask(Addr vpn, LegacyAcc la) override;
            };

            class PDPCache: public BaseTranslationCache
            {
                public:
                    PDPCache(std::string _name, uint32_t _size)
                        : BaseTranslationCache(_name, _size, 12, 30) {}
                    Addr legacyMask(Addr vpn, LegacyAcc la) override;
            };

            class PDECache: public BaseTranslationCache
            {
                public:
                    PDECache(std::string _name, uint32_t _size)
                        : BaseTranslationCache(_name, _size, 12, 21) {}
                    Addr legacyMask(Addr vpn, LegacyAcc la) override;
            };
        public:
            /** This class itself is a combination of caches */
            PML4Cache pml4Cache;
            PDPCache pdpCache;
            PDECache pdeCache;
        public:
            /** Constructor and destructor */
            PageStructureCache(std::string ownerName,
                uint32_t pml4c_size, uint32_t pdpc_size,
                uint32_t pdec_size)
                : pml4Cache(ownerName + ".pml4Cache", pml4c_size),
                    pdpCache(ownerName + ".pdpCache", pdpc_size),
                    pdeCache(ownerName + ".pdeCache", pdec_size) {}
            ~PageStructureCache() {}
        public:
            void flush();
            //PageTableEntry lookup(Addr va, PageWalkState state);
            //void insert(Addr vpn, const PageTableEntry& ptentry,
            //        PageWalkState state);
    };

} // namespace X86ISA
} // namespace gem5

#endif // __ARCH_X86_TRANSLATION_CACHE_HH__
