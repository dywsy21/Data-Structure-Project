#include "k-dtree.h"
#include <algorithm>
#include <cmath>

KdTree::KdTree(int k) : k(k), root(nullptr), tree_size(0) {}

KdTree::~KdTree() {}

void KdTree::insert(const std::vector<double>& point) {
    points.push_back(point);
    root = insertRec(std::move(root), point, 0);
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

void KdTree::insertEdge(uint32_t from, uint32_t to, double weight) {
    Node* node = root.get();
    while (node) {
        if (node->point[0] == from) {
            node->edges.emplace_back(to, weight);
            return;
        }
        node = (from < node->point[0]) ? node->left.get() : node->right.get();
    }
}

const std::vector<std::pair<uint32_t, double>>& KdTree::getEdges(uint32_t node) const {
    const Node* current = root.get();
    while (current) {
        if (current->point[0] == node) {
            return current->edges;
        }
        current = (node < current->point[0]) ? current->left.get() : current->right.get();
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

std::unique_ptr<KdTree::Node> KdTree::insertRec(std::unique_ptr<Node> node, const std::vector<double>& point, int depth) {
    if (!node) return std::make_unique<Node>(point);

    int cd = depth % k;
    if (point[cd] < node->point[cd]) {
        node->left = insertRec(std::move(node->left), point, depth + 1);
    } else {
        node->right = insertRec(std::move(node->right), point, depth + 1);
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
