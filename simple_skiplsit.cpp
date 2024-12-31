#pragma once

#include <iostream>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <vector>

/**
 * @brief 节点类，表示跳表中的一个节点
 * 
 * @note 该跳表主要为了解跳表的工作原理，没有使用模板定义排序器和存储的值类型，只是实现了简单的int存储和升序排序
 *       生产环境中可直接使用concurrent-skiplist
 */
class Node {
public:
    int value;  ///< 节点的值
    std::vector<Node*> forward;  ///< 指向不同层级的下一个节点的指针数组

    /**
     * @brief 构造函数
     * @param level 节点的层级
     * @param value 节点的值
     */
    Node(int level, int value) : value(value), forward(level + 1, nullptr) {}
};

/**
 * @brief 跳表类
 */
class SkipList {
private:
    int maxLevel;  ///< 跳表的最大层级
    float probability;  ///< 节点提升层级的概率
    int currentLevel;  ///< 当前跳表的最高层级
    Node* header;  ///< 跳表的头节点

    /**
     * @brief 生成一个随机层级
     * @return 随机生成的层级
     */
    int randomLevel() {
        int level = 0;
        while (static_cast<float>(rand()) / RAND_MAX < probability && level < maxLevel) {
            level++;
        }
        return level;
    }

    /**
     * @brief 查找大于或等于指定值的节点
     * @param value 指定的值
     * @param update 存储每一层中最后一个小于指定值的节点
     */
    void findGreaterOrEqual(int value, std::vector<Node*>& update) {
        Node* current = header;
        for (int i = currentLevel; i >= 0; i--) {
            while (current->forward[i] && current->forward[i]->value < value) {
                current = current->forward[i];
            }
            update[i] = current;
        }
    }

public:
    /**
     * @brief 构造函数
     * @param maxLevel 跳表的最大层级
     * @param probability 节点提升层级的概率
     */
    SkipList(int maxLevel, float probability) : maxLevel(maxLevel), probability(probability), currentLevel(0) {
        header = new Node(maxLevel, -1);
        srand(static_cast<unsigned>(time(nullptr)));
    }

    /**
     * @brief 析构函数
     */
    ~SkipList() {
        Node* current = header;
        while (current) {
            Node* next = current->forward[0];
            delete current;
            current = next;
        }
    }

    /**
     * @brief 插入一个值到跳表中
     * @param value 要插入的值
     */
    void insert(int value) {
        std::vector<Node*> update(maxLevel + 1, header);
        findGreaterOrEqual(value, update);

        Node* current = update[0]->forward[0];

        if (!current || current->value != value) {
            int newLevel = randomLevel();
            currentLevel = std::max(currentLevel, newLevel);

            Node* newNode = new Node(newLevel, value);
            for (int i = 0; i <= newLevel; i++) {
                newNode->forward[i] = update[i]->forward[i];
                update[i]->forward[i] = newNode;
            }
        }
    }

    /**
     * @brief 搜索跳表中是否存在指定值
     * @param value 要搜索的值
     * @return 如果存在返回true，否则返回false
     */
    bool search(int value) {
        std::vector<Node*> update(maxLevel + 1);
        findGreaterOrEqual(value, update);

        Node* current = update[0]->forward[0];
        return current && current->value == value;
    }

    /**
     * @brief 从跳表中删除指定值
     * @param value 要删除的值
     */
    void remove(int value) {
        std::vector<Node*> update(maxLevel + 1);
        findGreaterOrEqual(value, update);

        Node* current = update[0]->forward[0];

        if (current && current->value == value) {
            for (int i = 0; i <= currentLevel; i++) {
                if (update[i]->forward[i] != current) {
                    break;
                }
                update[i]->forward[i] = current->forward[i];
            }
            delete current;

            while (currentLevel > 0 && header->forward[currentLevel] == nullptr) {
                currentLevel--;
            }
        }
    }

    /**
     * @brief 显示跳表的内容
     */
    void display() {
        for (int i = currentLevel; i >= 0; i--) {
            Node* node = header->forward[i];
            std::cout << "Level " << i << ": ";
            while (node) {
                std::cout << node->value << " ";
                node = node->forward[i];
            }
            std::cout << std::endl;
        }
    }
};

int main() {
    SkipList list(5, 0.5);

    list.insert(3);
    list.insert(6);
    list.insert(7);
    list.insert(9);
    list.insert(12);
    list.insert(19);
    list.insert(17);
    list.insert(26);
    list.insert(21);
    list.insert(25);

    std::cout << "SkipList after insertion:" << std::endl;
    list.display();

    std::cout << "Search for 19: " << (list.search(19) ? "Found" : "Not Found") << std::endl;
    std::cout << "Search for 15: " << (list.search(15) ? "Found" : "Not Found") << std::endl;

    list.remove(19);
    std::cout << "SkipList after removing 19:" << std::endl;
    list.display();

    return 0;
}