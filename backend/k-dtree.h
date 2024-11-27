#ifndef KDTREE_H
#define KDTREE_H

#include <vector>
#include <memory>
#include <unordered_map>

class KdTree {
public:
    KdTree(int k);
    ~KdTree();

    void insert(const std::vector<double>& point);
    bool search(const std::vector<double>& point) const;
    std::vector<double> findMin(int dim) const;
    void deletePoint(const std::vector<double>& point);
    std::vector<double> findNearestNeighbor(const std::vector<double>& point) const;
    void insertEdge(uint32_t from, uint32_t to, double weight);
    const std::vector<std::pair<uint32_t, double>>& getEdges(uint32_t node) const;
    size_t size() const;
    void reserve(size_t n);
    const std::vector<double>& getPoint(uint32_t index) const;

private:
    struct Node {
        std::vector<double> point;
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;
        std::vector<std::pair<uint32_t, double>> edges;

        Node(const std::vector<double>& point) : point(point), left(nullptr), right(nullptr) {}
    };

    std::unique_ptr<Node> root;
    int k;
    size_t tree_size;
    std::vector<std::vector<double>> points; // Add this line to declare the points member variable

    std::unique_ptr<Node> insertRec(std::unique_ptr<Node> node, const std::vector<double>& point, int depth);
    bool searchRec(const Node* node, const std::vector<double>& point, int depth) const;
    std::unique_ptr<Node> deleteRec(std::unique_ptr<Node> node, const std::vector<double>& point, int depth);
    const Node* findMinRec(const Node* node, int dim, int depth) const;
    const Node* findNearestNeighborRec(const Node* node, const std::vector<double>& point, int depth, const Node* best, double& bestDist) const;
};

#endif // KDTREE_H
