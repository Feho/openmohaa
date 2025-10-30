// tests/test_yaml_cpp.cpp
// Added in OPM - Phase 2 Task 2.0
// Tests yaml-cpp integration

#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <string>

class YamlCppTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create a test YAML file
        std::ofstream testFile("test_config.yaml");
        testFile << "test:\n";
        testFile << "  name: test_bot\n";
        testFile << "  value: 42\n";
        testFile << "  flag: true\n";
        testFile << "  range: 3.14\n";
        testFile.close();
    }

    void TearDown() override
    {
        // Clean up test file
        std::remove("test_config.yaml");
    }
};

TEST_F(YamlCppTest, BasicParsing)
{
    YAML::Node config = YAML::LoadFile("test_config.yaml");

    ASSERT_TRUE(config["test"]);
    EXPECT_EQ(config["test"]["name"].as<std::string>(), "test_bot");
    EXPECT_EQ(config["test"]["value"].as<int>(), 42);
    EXPECT_TRUE(config["test"]["flag"].as<bool>());
    EXPECT_NEAR(config["test"]["range"].as<float>(), 3.14f, 0.01f);
}

TEST_F(YamlCppTest, NestedNodes)
{
    YAML::Node config = YAML::LoadFile("test_config.yaml");

    YAML::Node testNode = config["test"];
    EXPECT_TRUE(testNode.IsMap());
    EXPECT_EQ(testNode.size(), 4);
}

TEST_F(YamlCppTest, MissingKey)
{
    YAML::Node config = YAML::LoadFile("test_config.yaml");

    EXPECT_FALSE(config["nonexistent"]);
}
