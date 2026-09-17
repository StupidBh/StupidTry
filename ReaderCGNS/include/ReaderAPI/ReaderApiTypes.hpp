#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>

#include <stdexcept>

namespace ReaderAPI {
    using Integer = std::int32_t;
    using Real = float;

    struct Node
    {
        Integer id;
        Real x;
        Real y;
        Real z;
    };

    struct Elem
    {
        Integer id;
        Integer type;
        Integer npts;

        std::vector<Integer> nodes;
        std::vector<Integer> mid_nodes;    // 高阶单元的中间节点
        std::vector<Integer> corner_nodes; // 原始连通性（未来作单元映射时有用）
    };

    struct Field
    {
        std::string name;
        Integer type; // 0-Node | 1-CellCenter | 2-FaceCenter | 3-PointSet

        std::vector<Integer> ids;
        std::vector<Real> values;

        bool isEmpty() const noexcept { return this->name.empty() || this->ids.empty() || this->values.empty() || this->ids.size() != this->values.size(); }
    };

    class ElementNodeRange {
    public:
        ElementNodeRange(const Integer* values, std::size_t count) :
            m_values(count ? values : ElementNodeRange::EmptyData()),
            m_count(count)
        {
        }

        explicit ElementNodeRange(const std::vector<Integer>& values) :
            ElementNodeRange(values.data(), values.size())
        {
        }

        std::size_t size() const noexcept { return this->m_count; }

        bool empty() const noexcept { return this->m_count == 0; }

        const Integer* data() const noexcept { return this->m_values; }

        const Integer* begin() const noexcept { return this->m_values; }

        const Integer* end() const noexcept { return this->m_count ? this->m_values + this->m_count : this->m_values; }

        const Integer& operator[](std::size_t index) const noexcept { return this->m_values[index]; }

    private:
        static const Integer* EmptyData()
        {
            static constexpr Integer empty_data = 0;
            return &empty_data;
        }

        const Integer* m_values = ElementNodeRange::EmptyData();
        std::size_t m_count = 0;
    };

    struct ElementMetaData
    {
        Integer id;
        Integer type;
        Integer npts;
    };

    struct ElementView : ElementMetaData
    {
        ElementNodeRange nodes, mid_nodes, corner_nodes;

        explicit ElementView(const Elem& elem) :
            ElementMetaData { .id = elem.id, .type = elem.type, .npts = elem.npts },
            nodes(elem.nodes),
            mid_nodes(elem.mid_nodes),
            corner_nodes(elem.corner_nodes)
        {
        }

        ElementView(const ElementMetaData& meta_data, ElementNodeRange nodes, ElementNodeRange mid_nodes, ElementNodeRange corner_nodes) :
            ElementMetaData(meta_data),
            nodes(nodes),
            mid_nodes(mid_nodes),
            corner_nodes(corner_nodes)
        {
        }

        Elem Copy() const
        {
            Elem result { .id = this->id, .type = this->type, .npts = this->npts };
            if (!nodes.empty()) {
                result.nodes.assign_range(this->nodes);
            }
            if (!mid_nodes.empty()) {
                result.mid_nodes.assign_range(this->mid_nodes);
            }
            if (!corner_nodes.empty()) {
                result.corner_nodes.assign_range(this->corner_nodes);
            }
            return result;
        }
    };

    class ElementTable {
        struct Record
        {
            ElementMetaData meta_data;
            std::array<std::size_t, 4> offsets { };
        };

    public:
        template<class Counts>
        void Allocate(std::size_t count, Counts counts)
        {
            std::vector<Record> records(count);
            std::size_t total = 0;

            for (std::size_t i = 0; i < count; ++i) {
                const auto sizes = counts(i);
                auto& offsets = records[i].offsets;
                offsets[0] = total;

                for (std::size_t domain = 0; domain < 3; ++domain) {
                    if (sizes[domain] > std::numeric_limits<std::size_t>::max() - total) {
                        throw std::length_error("overflow");
                    }
                    total += sizes[domain];
                    offsets[domain + 1] = total;
                }
            }

            std::vector<Integer> node_ids(total);
            this->m_records.swap(records);
            this->m_node_ids.swap(node_ids);
        }

        void Assign(const std::vector<Elem>& elems)
        {
            ElementTable next;
            next.Allocate(elems.size(), [&](std::size_t i) {
                return std::array<std::size_t, 3> { elems[i].nodes.size(), elems[i].mid_nodes.size(), elems[i].corner_nodes.size() };
            });

            for (std::size_t i = 0; i < elems.size(); ++i) {
                const ElementView view(elems[i]);
                next.MetaData(i) = static_cast<ElementMetaData>(view);
                const ElementNodeRange domains[] = { view.nodes, view.mid_nodes, view.corner_nodes };
                for (std::size_t d = 0; d < 3; ++d) {
                    if (!domains[d].empty()) {
                        std::ranges::copy(domains[d], next.WriteNodeIds(i, d));
                    }
                }
            }
            *this = std::move(next);
        }

        std::vector<Elem> CopyElements() const
        {
            std::vector<Elem> result;
            result.reserve(this->size());
            for (std::size_t i = 0; i < this->size(); ++i) {
                result.emplace_back((*this)[i].Copy());
            }
            return result;
        }

        void Replace(std::size_t index, const Elem& elem)
        {
            if (index >= this->size()) {
                throw std::out_of_range("Element index out of range");
            }

            const auto previous = (*this)[index];
            const ElementView replacement(elem);

            if (previous.nodes.size() == replacement.nodes.size() && previous.mid_nodes.size() == replacement.mid_nodes.size() &&
                previous.corner_nodes.size() == replacement.corner_nodes.size()) {
                this->MetaData(index) = replacement;

                const ElementNodeRange domains[] = { replacement.nodes, replacement.mid_nodes, replacement.corner_nodes };
                for (std::size_t d = 0; d < 3; ++d) {
                    if (!domains[d].empty()) {
                        std::ranges::copy(domains[d], this->WriteNodeIds(index, d));
                    }
                }
                return;
            }

            ElementTable next;
            auto source = [&](std::size_t i) {
                return i == index ? ElementView(elem) : (*this)[i];
            };
            next.Allocate(this->size(), [&](std::size_t i) {
                const auto view = source(i);
                return std::array<std::size_t, 3> { view.nodes.size(), view.mid_nodes.size(), view.corner_nodes.size() };
            });

            for (std::size_t i = 0; i < this->size(); ++i) {
                const auto view = source(i);
                next.MetaData(i) = view;
                const ElementNodeRange domains[] = { view.nodes, view.mid_nodes, view.corner_nodes };
                for (std::size_t d = 0; d < 3; ++d) {
                    if (!domains[d].empty()) {
                        std::ranges::copy(domains[d], next.WriteNodeIds(i, d));
                    }
                }
            }
            *this = std::move(next);
        }

        void Append(ElementTable&& other)
        {
            if (this->empty()) {
                *this = std::move(other);
                return;
            }
            if (other.empty()) {
                return;
            }
            if (this == &other) {
                throw std::invalid_argument("Can't append table to itself");
            }
            if (other.m_node_ids.size() > this->m_node_ids.max_size() - this->m_node_ids.size() ||
                other.m_records.size() > this->m_records.max_size() - this->m_records.size()) {
                throw std::length_error("Element table append overflow");
            }

            const std::size_t base = this->m_node_ids.size();
            auto capacityFor = [](std::size_t required, std::size_t capacity, std::size_t maximum) {
                if (required <= capacity) {
                    return capacity;
                }
                return std::max(required, capacity > maximum / 2 ? maximum : capacity * 2);
            };

            this->m_records.reserve(capacityFor(this->m_records.size() + other.m_records.size(), this->m_records.capacity(), this->m_records.max_size()));
            this->m_node_ids.reserve(capacityFor(this->m_node_ids.size() + other.m_node_ids.size(), this->m_node_ids.capacity(), this->m_node_ids.max_size()));
            this->m_node_ids.insert(this->m_node_ids.end(), other.m_node_ids.begin(), other.m_node_ids.end());

            for (auto record : other.m_records) {
                for (auto& offset : record.offsets) {
                    offset += base;
                }
                this->m_records.emplace_back(record);
            }
            other.clear();
        }

        void clear()
        {
            this->m_records.clear();
            this->m_node_ids.clear();
        }

        ElementView at(std::size_t i) const
        {
            if (i >= this->size()) {
                throw std::out_of_range("Element index out of range");
            }
            return (*this)[i];
        }

        class Iterator {
        public:
            Iterator(const ElementTable* table, std::size_t index) :
                m_table(table),
                m_index(index)
            {
            }

            ElementView operator*() const { return (*this->m_table)[this->m_index]; }

            Iterator operator++()
            {
                ++this->m_index;
                return *this;
            }

            bool operator!=(const Iterator& other) const { return this->m_table != other.m_table || this->m_index != other.m_index; }

        private:
            const ElementTable* m_table;
            std::size_t m_index;
        };

        Iterator begin() const { return Iterator(this, 0); }

        Iterator end() const { return Iterator(this, this->size()); }

        std::size_t size() const noexcept { return this->m_records.size(); }

        bool empty() const noexcept { return this->m_records.empty(); }

        ElementMetaData& MetaData(std::size_t index) { return this->m_records[index].meta_data; }

        Integer* WriteNodeIds(std::size_t i, std::size_t domain)
        {
            const auto offset = this->m_records[i].offsets[domain];
            return this->m_node_ids.empty() ? nullptr : this->m_node_ids.data() + offset;
        }

        ElementView operator[](std::size_t i) const
        {
            const auto& record = this->m_records[i];
            auto range = [&](std::size_t domain) {
                const std::size_t first = record.offsets[domain];
                const std::size_t count = record.offsets[domain + 1] - first;
                return ElementNodeRange(count ? this->m_node_ids.data() + first : nullptr, count);
            };
            return ElementView(record.meta_data, range(0), range(1), range(2));
        }

    private:
        std::vector<Record> m_records;
        std::vector<Integer> m_node_ids;
    };

    class ElementRange {
    public:
        ElementRange(const std::vector<Elem>& elems) :
            m_elems(&elems)
        {
        }

        ElementRange(const ElementTable& table) :
            m_table(&table)
        {
        }

        std::size_t size() const noexcept { return this->m_table ? this->m_table->size() : this->m_elems->size(); }

        bool empty() const noexcept { return this->size() == 0; }

        ElementView operator[](std::size_t i) const { return this->m_table ? (*this->m_table)[i] : ElementView(this->m_elems->operator[](i)); }

        ElementView at(std::size_t i) const
        {
            if (i >= this->size()) {
                throw std::out_of_range("Element index out of range");
            }
            return (*this)[i];
        }

        class Iterator {
        public:
            Iterator(const ElementRange* range, std::size_t index) :
                m_range(range),
                m_index(index)
            {
            }

            ElementView operator*() const { return (*this->m_range)[this->m_index]; }

            Iterator operator++()
            {
                ++this->m_index;
                return *this;
            }

            bool operator!=(const Iterator& other) const { return this->m_range != other.m_range || this->m_index != other.m_index; }

        private:
            const ElementRange* m_range;
            std::size_t m_index;
        };

        Iterator begin() const { return Iterator(this, 0); }

        Iterator end() const { return Iterator(this, this->size()); }

    private:
        const std::vector<Elem>* m_elems = nullptr;
        const ElementTable* m_table = nullptr;
    };

} // namespace ReaderAPI
