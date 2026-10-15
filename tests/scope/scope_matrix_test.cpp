// Guard for the frozen MVP support matrix (TF-002).
//
// RED evidence for TF-002: this suite fails while the machine-readable capability
// table is missing, and fails again while docs/SCOPE.md is out of lockstep with it.
// The classifier below is deliberately test-local: production enforcement of the
// same table lands with TF-015 (shape inference) and TF-016 (canonical JSON
// importer), and the diagnostic codes themselves are TF-028.
#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace tgvc::scope {
namespace {

constexpr char kTableRelativePath[] = "schemas/tgvc-operators-v1.json";
constexpr char kScopeRelativePath[] = "docs/SCOPE.md";

const std::vector<std::string> kCoreOperators = {
    "Constant", "Add", "Mul", "ReLU", "MatMul", "Reshape", "Transpose"};
const std::vector<std::string> kElementwiseBinary = {"Add", "Mul"};
const std::vector<std::string> kMandatedNonGoals = {"dynamic_shapes", "control_flow", "training",
                                                    "full_onnx",      "graphics",     "proprietary_targets"};
const std::vector<std::string> kRejectionReasons = {"unsupported_operator", "unsupported_dtype",
                                                    "unsupported_rank",     "dynamic_dimension",
                                                    "unsupported_attribute", "malformed_graph"};

std::string SourcePath(const std::string& relative) {
  return std::string(TGVC_SOURCE_ROOT) + "/" + relative;
}

std::string ReadFile(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return {};
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

// Returns an empty object when the file is unreadable or not valid JSON, after
// recording a failure with the path so the RED output names the missing artifact.
nlohmann::json LoadJson(const std::string& relative) {
  const std::string path = SourcePath(relative);
  const std::string text = ReadFile(path);
  if (text.empty()) {
    ADD_FAILURE() << "cannot read " << path;
    return nlohmann::json::object();
  }
  try {
    return nlohmann::json::parse(text);
  } catch (const std::exception& error) {
    ADD_FAILURE() << "cannot parse " << path << ": " << error.what();
    return nlohmann::json::object();
  }
}

// Safe child lookup: a missing key yields an empty object instead of throwing, so a
// broken table produces assertion failures rather than uncaught exceptions.
const nlohmann::json& Child(const nlohmann::json& parent, const std::string& key) {
  static const nlohmann::json kEmpty = nlohmann::json::object();
  if (!parent.is_object() || !parent.contains(key)) {
    return kEmpty;
  }
  return parent.at(key);
}

std::vector<std::string> StringList(const nlohmann::json& value) {
  std::vector<std::string> items;
  if (!value.is_array()) {
    return items;
  }
  for (const auto& entry : value) {
    if (entry.is_string()) {
      items.push_back(entry.get<std::string>());
    }
  }
  return items;
}

// Test-local reference policy for the four mandated negative cases: unsupported
// operator, dynamic dimension, unsupported rank and unsupported element type.
// Returns the rejection category, or an empty string when the node is accepted.
std::string Classify(const nlohmann::json& matrix, const std::string& op_name, const std::string& dtype,
                     const std::vector<nlohmann::json>& shape) {
  bool known_operator = false;
  for (const auto& entry : Child(matrix, "operators")) {
    if (entry.value("name", "") == op_name) {
      known_operator = true;
      break;
    }
  }
  if (!known_operator) {
    return "unsupported_operator";
  }
  const auto supported_dtypes = StringList(Child(Child(matrix, "dtypes"), "supported"));
  if (std::find(supported_dtypes.begin(), supported_dtypes.end(), dtype) == supported_dtypes.end()) {
    return "unsupported_dtype";
  }
  for (const auto& dim : shape) {
    if (!dim.is_number_integer()) {
      return "dynamic_dimension";
    }
    if (dim.get<long long>() <= 0) {
      return "malformed_graph";
    }
  }
  const auto& rank = Child(Child(matrix, "shape_policy"), "rank");
  const auto shape_rank = static_cast<long long>(shape.size());
  if (shape_rank < rank.value("min", 0LL) || shape_rank > rank.value("max", 0LL)) {
    return "unsupported_rank";
  }
  return {};
}

std::string Trim(const std::string& text) {
  const auto begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return {};
  }
  const auto end = text.find_last_not_of(" \t\r\n");
  return text.substr(begin, end - begin + 1);
}

// Returns the bare name when the cell is exactly one `backticked` token, else "".
std::string BacktickedToken(const std::string& cell) {
  const std::string trimmed = Trim(cell);
  if (trimmed.size() < 3 || trimmed.front() != '`' || trimmed.back() != '`') {
    return {};
  }
  const std::string token = trimmed.substr(1, trimmed.size() - 2);
  if (token.find('`') != std::string::npos) {
    return {};
  }
  return token;
}

std::vector<std::string> SplitCells(const std::string& line) {
  std::string trimmed = Trim(line);
  if (!trimmed.empty() && trimmed.front() == '|') {
    trimmed.erase(trimmed.begin());
  }
  if (!trimmed.empty() && trimmed.back() == '|') {
    trimmed.pop_back();
  }
  std::vector<std::string> cells;
  std::istringstream stream(trimmed);
  std::string cell;
  while (std::getline(stream, cell, '|')) {
    cells.push_back(Trim(cell));
  }
  return cells;
}

struct ScopeRow {
  std::string name;
  std::string category;
  std::string arity;
  std::string shape_rule;
  std::string rank_rule;
  std::string attributes;
};

// Parses the data rows of the operator table between the marker comments in
// docs/SCOPE.md, so prose and table cannot drift apart.
std::vector<ScopeRow> ScopeRows(const std::string& scope_text) {
  const std::string begin_marker = "<!-- operator-matrix:start -->";
  const std::string end_marker = "<!-- operator-matrix:end -->";
  const auto begin = scope_text.find(begin_marker);
  const auto end = scope_text.find(end_marker);
  if (begin == std::string::npos || end == std::string::npos || end < begin) {
    return {};
  }
  const std::string block =
      scope_text.substr(begin + begin_marker.size(), end - begin - begin_marker.size());
  std::vector<ScopeRow> rows;
  std::istringstream stream(block);
  std::string line;
  while (std::getline(stream, line)) {
    const auto cells = SplitCells(line);
    if (cells.size() != 7) {
      continue;
    }
    const std::string name = BacktickedToken(cells[0]);
    if (name.empty()) {
      continue;  // header and separator rows do not carry a backticked name
    }
    rows.push_back({name, cells[1], cells[2], cells[3], cells[4], cells[5]});
  }
  return rows;
}

class MatrixTest : public ::testing::Test {
 protected:
  void SetUp() override {
    matrix_ = LoadJson(kTableRelativePath);
    operators_ = Child(matrix_, "operators").is_array() ? Child(matrix_, "operators")
                                                        : nlohmann::json::array();
    scope_text_ = ReadFile(SourcePath(kScopeRelativePath));
  }

  const nlohmann::json& OperatorNamed(const std::string& name) const {
    for (const auto& entry : operators_) {
      if (entry.value("name", "") == name) {
        return entry;
      }
    }
    ADD_FAILURE() << "operator " << name << " is missing from the capability table";
    static const nlohmann::json kEmpty = nlohmann::json::object();
    return kEmpty;
  }

  std::vector<std::string> OperatorNames() const {
    std::vector<std::string> names;
    for (const auto& entry : operators_) {
      names.push_back(entry.value("name", ""));
    }
    return names;
  }

  nlohmann::json matrix_;
  nlohmann::json operators_;
  std::string scope_text_;
};

TEST_F(MatrixTest, TableIsFrozenAndVersioned) {
  ASSERT_FALSE(matrix_.empty()) << "capability table is missing";
  EXPECT_EQ("frozen", matrix_.value("status", ""));
  EXPECT_EQ("TF-002", matrix_.value("frozen_by", ""));
  EXPECT_FALSE(matrix_.value("schema_version", "").empty());
  EXPECT_FALSE(matrix_.value("matrix_version", "").empty());
}

TEST_F(MatrixTest, DeclaresExactlyTheSevenCoreOperators) {
  ASSERT_FALSE(matrix_.empty()) << "capability table is missing";
  EXPECT_EQ(kCoreOperators, OperatorNames());
  for (const auto& entry : operators_) {
    EXPECT_EQ("core", entry.value("status", "")) << entry.value("name", "");
  }
}

TEST_F(MatrixTest, EveryOperatorRecordIsComplete) {
  ASSERT_FALSE(matrix_.empty()) << "capability table is missing";
  const std::vector<std::string> required_keys = {"name",           "category",  "status",
                                                  "arity",          "shape_rule", "rank_rule",
                                                  "dtype_rule",     "attributes", "constraints",
                                                  "rejected_when"};
  for (const auto& entry : operators_) {
    const std::string name = entry.value("name", "");
    for (const auto& key : required_keys) {
      EXPECT_TRUE(entry.contains(key)) << name << " is missing key " << key;
    }
    EXPECT_EQ(1, Child(entry, "arity").value("outputs", 0)) << name;
    EXPECT_FALSE(StringList(Child(entry, "rejected_when")).empty()) << name;
  }
}

TEST_F(MatrixTest, ElementwiseOperatorsRejectBroadcasting) {
  ASSERT_FALSE(matrix_.empty()) << "capability table is missing";
  for (const auto& name : kElementwiseBinary) {
    const auto& entry = OperatorNamed(name);
    const auto& constraints = Child(entry, "constraints");
    EXPECT_EQ("identical_shapes_only", entry.value("shape_rule", "")) << name;
    EXPECT_EQ("rejected", constraints.value("broadcasting", "")) << name;
    const auto rejected_when = StringList(Child(entry, "rejected_when"));
    EXPECT_NE(rejected_when.end(), std::find(rejected_when.begin(), rejected_when.end(), "broadcast_required"))
        << name;
  }
  EXPECT_EQ("rejected", Child(matrix_, "elementwise_policy").value("broadcasting", ""));
}

TEST_F(MatrixTest, MatMulIsRankTwoWithEqualInnerDimensions) {
  ASSERT_FALSE(matrix_.empty()) << "capability table is missing";
  const auto& entry = OperatorNamed("MatMul");
  const auto& constraints = Child(entry, "constraints");
  EXPECT_EQ("2", entry.value("rank_rule", ""));
  EXPECT_EQ(2, constraints.value("operand_rank", 0));
  EXPECT_TRUE(constraints.value("k_dimensions_equal", false));
}

TEST_F(MatrixTest, ReshapePreservesElementCountWithOneInferredDim) {
  ASSERT_FALSE(matrix_.empty()) << "capability table is missing";
  const auto& entry = OperatorNamed("Reshape");
  const auto& constraints = Child(entry, "constraints");
  EXPECT_EQ("element_count_preserved", entry.value("shape_rule", ""));
  EXPECT_EQ(1, constraints.value("max_inferred_dims", 0));
  EXPECT_EQ(-1, constraints.value("inferred_dim_sentinel", 0));
  EXPECT_EQ(std::vector<std::string>({"shape"}), StringList(Child(entry, "attributes")));
}

TEST_F(MatrixTest, TransposeRequiresCompleteUniquePermutation) {
  ASSERT_FALSE(matrix_.empty()) << "capability table is missing";
  const auto& entry = OperatorNamed("Transpose");
  const auto& constraints = Child(entry, "constraints");
  EXPECT_EQ("complete_unique_permutation", entry.value("shape_rule", ""));
  EXPECT_TRUE(constraints.value("perm_length_equals_rank", false));
  EXPECT_TRUE(constraints.value("perm_unique", false));
  EXPECT_EQ(std::vector<std::string>({"perm"}), StringList(Child(entry, "attributes")));
}

TEST_F(MatrixTest, ConstantIsALeafWithStaticShape) {
  ASSERT_FALSE(matrix_.empty()) << "capability table is missing";
  const auto& entry = OperatorNamed("Constant");
  const auto& constraints = Child(entry, "constraints");
  EXPECT_EQ(0, Child(entry, "arity").value("inputs", -1));
  EXPECT_EQ("shape_from_attribute", entry.value("shape_rule", ""));
  EXPECT_EQ(std::vector<std::string>({"value", "shape"}), StringList(Child(entry, "attributes")));
  EXPECT_EQ("static_positive", constraints.value("shape", ""));
  EXPECT_EQ("equals_shape_product", constraints.value("literal_element_count", ""));
}

TEST_F(MatrixTest, DtypePolicyIsFloat32Only) {
  ASSERT_FALSE(matrix_.empty()) << "capability table is missing";
  const auto& dtypes = Child(matrix_, "dtypes");
  EXPECT_EQ(std::vector<std::string>({"float32"}), StringList(Child(dtypes, "supported")));
  const auto rejected = StringList(Child(dtypes, "rejected"));
  for (const auto& dtype : {"float16", "bfloat16", "int8", "quantized"}) {
    EXPECT_NE(rejected.end(), std::find(rejected.begin(), rejected.end(), dtype)) << dtype;
  }
  EXPECT_EQ("none", dtypes.value("promotion", ""));
  EXPECT_EQ("rejected", dtypes.value("mixed_dtype", ""));
}

TEST_F(MatrixTest, ShapePolicyIsStaticRankedWithBounds) {
  ASSERT_FALSE(matrix_.empty()) << "capability table is missing";
  const auto& policy = Child(matrix_, "shape_policy");
  const auto& rank = Child(policy, "rank");
  const auto& dim = Child(policy, "dim");
  EXPECT_EQ("static_ranked", policy.value("kind", ""));
  EXPECT_EQ(1, rank.value("min", 0));
  EXPECT_EQ(4, rank.value("max", 0));
  EXPECT_EQ("positive_integer_literal", dim.value("kind", ""));
  EXPECT_EQ("rejected", dim.value("dynamic", ""));
  EXPECT_EQ("rejected", dim.value("symbolic", ""));
  EXPECT_EQ("rejected", dim.value("zero_sized", ""));
  EXPECT_GT(policy.value("max_elements_per_tensor", 0), 0);
}

TEST_F(MatrixTest, MandatedNegativeCasesAreRejected) {
  ASSERT_FALSE(matrix_.empty()) << "capability table is missing";
  EXPECT_EQ("unsupported_operator", Classify(matrix_, "Conv", "float32", {2, 2}));
  EXPECT_EQ("unsupported_dtype", Classify(matrix_, "Add", "float16", {2, 2}));
  EXPECT_EQ("unsupported_rank", Classify(matrix_, "Add", "float32", {1, 1, 1, 1, 1}));
  EXPECT_EQ("dynamic_dimension", Classify(matrix_, "Add", "float32", {2, "N"}));
  EXPECT_EQ("malformed_graph", Classify(matrix_, "Add", "float32", {0, 2}));
  EXPECT_EQ("", Classify(matrix_, "Add", "float32", {2, 2}));
}

TEST_F(MatrixTest, RejectionTaxonomyCoversEveryReasonOnce) {
  ASSERT_FALSE(matrix_.empty()) << "capability table is missing";
  const auto& taxonomy = Child(matrix_, "rejection_taxonomy");
  std::vector<std::string> reasons;
  for (const auto& entry : taxonomy) {
    reasons.push_back(entry.value("reason", ""));
    EXPECT_FALSE(entry.value("example", "").empty()) << entry.value("reason", "");
    EXPECT_FALSE(entry.value("enforced_by", "").empty()) << entry.value("reason", "");
  }
  auto sorted_reasons = reasons;
  auto sorted_expected = kRejectionReasons;
  std::sort(sorted_reasons.begin(), sorted_reasons.end());
  std::sort(sorted_expected.begin(), sorted_expected.end());
  EXPECT_EQ(sorted_expected, sorted_reasons);
  auto unique_reasons = sorted_reasons;
  unique_reasons.erase(std::unique(unique_reasons.begin(), unique_reasons.end()), unique_reasons.end());
  EXPECT_EQ(unique_reasons.size(), sorted_reasons.size());
}

TEST_F(MatrixTest, NonGoalsCoverTheMandatedSet) {
  ASSERT_FALSE(matrix_.empty()) << "capability table is missing";
  const auto non_goals = StringList(Child(matrix_, "non_goals"));
  for (const auto& goal : kMandatedNonGoals) {
    EXPECT_NE(non_goals.end(), std::find(non_goals.begin(), non_goals.end(), goal)) << goal;
  }
}

TEST_F(MatrixTest, DeferredDecisionsNameAnOwner) {
  ASSERT_FALSE(matrix_.empty()) << "capability table is missing";
  std::string owners;
  for (const auto& entry : Child(matrix_, "deferred_decisions")) {
    EXPECT_FALSE(entry.value("topic", "").empty());
    EXPECT_FALSE(entry.value("owner", "").empty()) << entry.value("topic", "");
    owners += entry.value("owner", "") + " ";
  }
  for (const auto& owner : {"TF-015", "TF-025", "TF-028", "TF-072"}) {
    EXPECT_NE(std::string::npos, owners.find(owner)) << owner;
  }
}

TEST_F(MatrixTest, ScopeDocumentMatrixMatchesTheTable) {
  ASSERT_FALSE(matrix_.empty()) << "capability table is missing";
  ASSERT_FALSE(scope_text_.empty()) << "docs/SCOPE.md is missing";
  const auto rows = ScopeRows(scope_text_);
  ASSERT_EQ(kCoreOperators.size(), rows.size()) << "docs/SCOPE.md operator table is missing rows";
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const auto& row = rows[index];
    const auto& entry = OperatorNamed(row.name);
    EXPECT_EQ(kCoreOperators[index], row.name);
    EXPECT_EQ("`" + entry.value("category", "") + "`", row.category) << row.name;
    EXPECT_EQ(std::to_string(Child(entry, "arity").value("inputs", -1)) + "→" +
                  std::to_string(Child(entry, "arity").value("outputs", -1)),
              row.arity)
        << row.name;
    EXPECT_EQ("`" + entry.value("shape_rule", "") + "`", row.shape_rule) << row.name;
    EXPECT_EQ("`" + entry.value("rank_rule", "") + "`", row.rank_rule) << row.name;
    const auto attributes = StringList(Child(entry, "attributes"));
    std::string expected_attributes;
    for (const auto& attribute : attributes) {
      expected_attributes += (expected_attributes.empty() ? "" : ", ") + ("`" + attribute + "`");
    }
    EXPECT_EQ(expected_attributes.empty() ? "—" : expected_attributes, row.attributes) << row.name;
  }
}

TEST_F(MatrixTest, ScopeDocumentRecordsTheReproducibleCheck) {
  ASSERT_FALSE(matrix_.empty()) << "capability table is missing";
  ASSERT_FALSE(scope_text_.empty()) << "docs/SCOPE.md is missing";
  const std::string check = matrix_.value("reproducible_check", "");
  ASSERT_FALSE(check.empty()) << "capability table has no reproducible_check";
  EXPECT_NE(std::string::npos, scope_text_.find(check)) << check;
}

}  // namespace
}  // namespace tgvc::scope
