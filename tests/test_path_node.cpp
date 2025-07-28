#include <gtest/gtest.h>
#include "jsom.hpp"

namespace jsom {

class PathNodeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PathNodeTest, DefaultConstruction) {
    PathNode node;
    EXPECT_TRUE(node.is_root());
    EXPECT_EQ(node.depth(), 0);
    EXPECT_EQ(node.first_child, nullptr);
    EXPECT_EQ(node.next_sibling, nullptr);
}

TEST_F(PathNodeTest, ConstructionWithComponent) {
    PathNode root;
    PathNode child(PathNode::ObjectKey, "test_key", &root);
    
    EXPECT_FALSE(child.is_root());
    EXPECT_EQ(child.component, "test_key");
    EXPECT_EQ(child.parent, &root);
    EXPECT_EQ(child.type, PathNode::ObjectKey);
}

TEST_F(PathNodeTest, AddChild) {
    PathNode root;
    PathNode* child1 = root.add_child(PathNode::ObjectKey, "key1");
    PathNode* child2 = root.add_child(PathNode::ArrayIndex, "0");
    
    EXPECT_EQ(root.first_child, child1);
    EXPECT_EQ(child1->next_sibling, child2);
    EXPECT_EQ(child2->next_sibling, nullptr);
    EXPECT_EQ(child1->parent, &root);
    EXPECT_EQ(child2->parent, &root);
}

TEST_F(PathNodeTest, FindChild) {
    PathNode root;
    root.add_child(PathNode::ObjectKey, "key1");
    root.add_child(PathNode::ObjectKey, "key2");
    
    PathNode* found = root.find_child("key2");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->component, "key2");
    
    PathNode* not_found = root.find_child("nonexistent");
    EXPECT_EQ(not_found, nullptr);
}

TEST_F(PathNodeTest, GenerateJsonPointer) {
    PathNode root;
    PathNode* level1 = root.add_child(PathNode::ObjectKey, "user");
    PathNode* level2 = level1->add_child(PathNode::ArrayIndex, "0");
    PathNode* level3 = level2->add_child(PathNode::ObjectKey, "name");
    
    EXPECT_EQ(root.generate_json_pointer(), "");
    EXPECT_EQ(level1->generate_json_pointer(), "/user");
    EXPECT_EQ(level2->generate_json_pointer(), "/user/0");
    EXPECT_EQ(level3->generate_json_pointer(), "/user/0/name");
}

TEST_F(PathNodeTest, JsonPointerEscaping) {
    PathNode root;
    PathNode* special_key = root.add_child(PathNode::ObjectKey, "key~with/special");
    
    // ~ should become ~0, / should become ~1
    EXPECT_EQ(special_key->generate_json_pointer(), "/key~0with~1special");
}

TEST_F(PathNodeTest, DepthCalculation) {
    PathNode root;
    PathNode* level1 = root.add_child(PathNode::ObjectKey, "a");
    PathNode* level2 = level1->add_child(PathNode::ObjectKey, "b");
    PathNode* level3 = level2->add_child(PathNode::ObjectKey, "c");
    
    EXPECT_EQ(root.depth(), 0);
    EXPECT_EQ(level1->depth(), 1);
    EXPECT_EQ(level2->depth(), 2);
    EXPECT_EQ(level3->depth(), 3);
}

} // namespace jsom