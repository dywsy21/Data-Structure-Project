#ifndef KDTREE_H
#define KDTREE_H

#include <vector>
#include <memory>
#include <unordered_map>
#include <queue>
#include <utility>
#include <iostream>
#include <functional>
#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/split_member.hpp>

class KdTree {
public:
    KdTree(int k);
    ~KdTree();

    void insert(const std::vector<double>& point, uint32_t index);
    bool search(const std::vector<double>& point) const;
    std::vector<double> findMin(int dim) const;
    void deletePoint(const std::vector<double>& point);
    std::vector<double> findNearestNeighbor(const std::vector<double>& point) const;
    void insertEdge(uint32_t from, uint32_t to, double weight);
    const std::vector<std::pair<uint32_t, double>>& getEdges(uint32_t node_index) const;
    size_t size() const;
    void reserve(size_t n);
    const std::vector<double>& getPoint(uint32_t index) const;
    std::vector<std::vector<double>> findKthNearestNeighbor(const std::vector<double>& point, int k) const;

    std::vector<std::vector<double>> points;

    void rebuildIndexMap();  // One-pass rebuild after load

private:
    struct Node {
        std::vector<double> point;
        uint32_t index;
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;
        std::vector<std::pair<uint32_t, double>> edges;

        Node(const std::vector<double>& point, uint32_t index)
            : point(point), index(index), left(nullptr), right(nullptr) {}

    private:
        friend class boost::serialization::access;
        template<class Archive>
        void serialize(Archive & ar, const unsigned int /*version*/) {
            ar & point;
            ar & index;
            ar & edges;
        }
    };

    std::unique_ptr<Node> root;
    int k;
    size_t tree_size;
    std::unordered_map<uint32_t, Node*> index_to_node;

    std::unique_ptr<Node> insertRec(std::unique_ptr<Node> node, const std::vector<double>& point, uint32_t index, int depth);
    bool searchRec(const Node* node, const std::vector<double>& point, int depth) const;
    std::unique_ptr<Node> deleteRec(std::unique_ptr<Node> node, const std::vector<double>& point, int depth);
    const Node* findMinRec(const Node* node, int dim, int depth) const;
    const Node* findNearestNeighborRec(const Node* node, const std::vector<double>& point, int depth, const Node* best, double& bestDist) const;
    void findKthNearestNeighborRec(const Node* node, const std::vector<double>& point, int depth, int k,
                                   std::priority_queue<std::pair<double, const Node*>>& max_heap) const;

    // Define a custom key type for the cache
    struct CacheKey {
        std::vector<double> point;
        int k;

        bool operator==(const CacheKey& other) const {
            return point == other.point && k == other.k;
        }
    };

    struct CacheKeyHash {
        std::size_t operator()(const CacheKey& key) const {
            std::size_t hash1 = std::hash<int>()(key.k);
            std::size_t hash2 = 0;
            for (const auto& val : key.point) {
                hash2 ^= std::hash<double>()(val) + 0x9e3779b9 + (hash2 << 6) + (hash2 >> 2);
            }
            return hash1 ^ hash2;
        }
    };

    mutable std::unordered_map<CacheKey, std::vector<std::vector<double>>, CacheKeyHash> cache;

    template<class Archive>
    void saveNode(Archive& ar, const Node* node) const;

    template<class Archive>
    std::unique_ptr<Node> loadNode(Archive& ar);

    friend class boost::serialization::access;
    
    template<class Archive>
    void save(Archive& ar, const unsigned int version) const;

    template<class Archive> 
    void load(Archive& ar, const unsigned int version);

    BOOST_SERIALIZATION_SPLIT_MEMBER()
};

template<class Archive>
void KdTree::load(Archive & ar, const unsigned int /*version*/) {
    ar & k;
    ar & tree_size;
    ar & points;
    bool hasRoot;
    ar & hasRoot;
    if (hasRoot) {
        root = loadNode(ar);
    }
    // Clear the cache to avoid unnecessary overhead
    cache.clear();
    // Rebuild the index map in one pass
    rebuildIndexMap();
}

#endif // KDTREE_H
