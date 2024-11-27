#include "k-dtree.h"
#include "defs.h"
#include <algorithm>
#include <cmath>

KdTree::KdTree(int k) : k(k), root(nullptr), tree_size(0) {}

KdTree::~KdTree() {}

void KdTree::insert(const std::vector<double>& point, uint32_t index) {
    points.push_back(point);
    root = insertRec(std::move(root), point, index, 0);
    ++tree_size;
}

bool KdTree::search(const std::vector<double>& point) const {
    return searchRec(root.get(), point, 0);
}

std::vector<double> KdTree::findMin(int dim) const {
    const Node* minNode = findMinRec(root.get(), dim, 0);
    return minNode ? minNode->point : std::vector<double>();
}

void KdTree::deletePoint(const std::vector<double>& point) {
    root = deleteRec(std::move(root), point, 0);
    --tree_size;
}

std::vector<double> KdTree::findNearestNeighbor(const std::vector<double>& point) const {
    double bestDist = std::numeric_limits<double>::max();
    const Node* best = findNearestNeighborRec(root.get(), point, 0, nullptr, bestDist);
    return best ? best->point : std::vector<double>();
}

std::vector<std::vector<double>> KdTree::findKthNearestNeighbors(const std::vector<double>& point, int k) const {
    std::priority_queue<std::pair<double, const Node*>> max_heap;
    findKthNearestNeighborsRec(root.get(), point, 0, k, max_heap);

    std::vector<std::vector<double>> result;
    while (!max_heap.empty()) {
        result.push_back(max_heap.top().second->point);
        max_heap.pop();
    }
    std::reverse(result.begin(), result.end());
    return result;
}

void KdTree::findKthNearestNeighborsRec(const Node* node, const std::vector<double>& point, int depth, int k,
                                        std::priority_queue<std::pair<double, const Node*>>& max_heap) const {
    if (!node) return;

    double dist = 0;
    for (int i = 0; i < this->k; ++i) { // Use this->k instead of k to ensure correct dimension
        dist += (node->point[i] - point[i]) * (node->point[i] - point[i]);
    }

    if (max_heap.size() < k) {
        max_heap.emplace(dist, node);
    } else if (dist < max_heap.top().first) {
        max_heap.pop();
        max_heap.emplace(dist, node);
    }

    int cd = depth % this->k; // Use this->k instead of k to ensure correct dimension
    const Node* nextNode = point[cd] < node->point[cd] ? node->left.get() : node->right.get();
    const Node* otherNode = point[cd] < node->point[cd] ? node->right.get() : node->left.get();

    findKthNearestNeighborsRec(nextNode, point, depth + 1, k, max_heap);

    if ((point[cd] - node->point[cd]) * (point[cd] - node->point[cd]) < max_heap.top().first) {
        findKthNearestNeighborsRec(otherNode, point, depth + 1, k, max_heap);
    }
}

void KdTree::insertEdge(uint32_t from, uint32_t to, double weight) {
    auto it = index_to_node.find(from);
    if (it != index_to_node.end()) {
        it->second->edges.emplace_back(to, weight);
    }
}

const std::vector<std::pair<uint32_t, double>>& KdTree::getEdges(uint32_t node_index) const {
    auto it = index_to_node.find(node_index);
    if (it != index_to_node.end()) {
        return it->second->edges;
    }
    static const std::vector<std::pair<uint32_t, double>> empty;
    return empty;
}

size_t KdTree::size() const {
    return tree_size;
}

void KdTree::reserve(size_t n) {
    points.reserve(n);
}

const std::vector<double>& KdTree::getPoint(uint32_t index) const {
    return points[index];
}

std::unique_ptr<KdTree::Node> KdTree::insertRec(std::unique_ptr<Node> node, const std::vector<double>& point, uint32_t index, int depth) {
    if (!node) {
        auto new_node = std::make_unique<Node>(point, index);
        index_to_node[index] = new_node.get(); // Store mapping
        return new_node;
    }
    int cd = depth % k;
    if (point[cd] < node->point[cd]) {
        node->left = insertRec(std::move(node->left), point, index, depth + 1);
    } else {
        node->right = insertRec(std::move(node->right), point, index, depth + 1);
    }
    return node;
}

bool KdTree::searchRec(const Node* node, const std::vector<double>& point, int depth) const {
    if (!node) return false;
    if (node->point == point) return true;

    int cd = depth % k;
    if (point[cd] < node->point[cd]) {
        return searchRec(node->left.get(), point, depth + 1);
    } else {
        return searchRec(node->right.get(), point, depth + 1);
    }
}

std::unique_ptr<KdTree::Node> KdTree::deleteRec(std::unique_ptr<Node> node, const std::vector<double>& point, int depth) {
    if (!node) return nullptr;

    int cd = depth % k;
    if (node->point == point) {
        if (node->right) {
            const Node* minNode = findMinRec(node->right.get(), cd, depth + 1);
            node->point = minNode->point;
            node->right = deleteRec(std::move(node->right), minNode->point, depth + 1);
        } else if (node->left) {
            const Node* minNode = findMinRec(node->left.get(), cd, depth + 1);
            node->point = minNode->point;
            node->right = deleteRec(std::move(node->left), minNode->point, depth + 1);
            node->left = nullptr;
        } else {
            return nullptr;
        }
    } else if (point[cd] < node->point[cd]) {
        node->left = deleteRec(std::move(node->left), point, depth + 1);
    } else {
        node->right = deleteRec(std::move(node->right), point, depth + 1);
    }
    return node;
}

const KdTree::Node* KdTree::findMinRec(const Node* node, int dim, int depth) const {
    if (!node) return nullptr;

    int cd = depth % k;
    if (cd == dim) {
        if (!node->left) return node;
        return findMinRec(node->left.get(), dim, depth + 1);
    }

    const Node* leftMin = findMinRec(node->left.get(), dim, depth + 1);
    const Node* rightMin = findMinRec(node->right.get(), dim, depth + 1);
    const Node* minNode = node;

    if (leftMin && leftMin->point[dim] < minNode->point[dim]) minNode = leftMin;
    if (rightMin && rightMin->point[dim] < minNode->point[dim]) minNode = rightMin;

    return minNode;
}

const KdTree::Node* KdTree::findNearestNeighborRec(const Node* node, const std::vector<double>& point, int depth, const Node* best, double& bestDist) const {
    if (!node) return best;

    double dist = 0;
    for (int i = 0; i < k; ++i) {
        dist += (node->point[i] - point[i]) * (node->point[i] - point[i]);
    }

    if (dist < bestDist) {
        bestDist = dist;
        best = node;
    }

    int cd = depth % k;
    const Node* nextNode = point[cd] < node->point[cd] ? node->left.get() : node->right.get();
    const Node* otherNode = point[cd] < node->point[cd] ? node->right.get() : node->left.get();

    best = findNearestNeighborRec(nextNode, point, depth + 1, best, bestDist);

    if ((point[cd] - node->point[cd]) * (point[cd] - node->point[cd]) < bestDist) {
        best = findNearestNeighborRec(otherNode, point, depth + 1, best, bestDist);
    }

    return best;
}
