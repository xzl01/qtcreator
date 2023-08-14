// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "abstractviewmock.h"
#include "googletest.h"
#include "projectstoragemock.h"

#include <model.h>
#include <modelnode.h>
#include <nodelistproperty.h>
#include <projectstorage/projectstorage.h>
#include <sqlitedatabase.h>

namespace {

using QmlDesigner::ModelNode;
using QmlDesigner::ModuleId;
using QmlDesigner::PropertyDeclarationId;
using QmlDesigner::TypeId;
namespace Info = QmlDesigner::Storage::Info;

class NodeListProperty : public testing::Test
{
protected:
    using iterator = QmlDesigner::NodeListProperty::iterator;
    NodeListProperty()
    {
        model->attachView(&abstractViewMock);
        nodeListProperty = abstractViewMock.rootModelNode().nodeListProperty("foo");

        node1 = abstractViewMock.createModelNode("QtQick.Item1", -1, -1);
        node2 = abstractViewMock.createModelNode("QtQick.Item2", -1, -1);
        node3 = abstractViewMock.createModelNode("QtQick.Item3", -1, -1);
        node4 = abstractViewMock.createModelNode("QtQick.Item4", -1, -1);
        node5 = abstractViewMock.createModelNode("QtQick.Item5", -1, -1);

        nodeListProperty.reparentHere(node1);
        nodeListProperty.reparentHere(node2);
        nodeListProperty.reparentHere(node3);
        nodeListProperty.reparentHere(node4);
        nodeListProperty.reparentHere(node5);
    }

    ~NodeListProperty() { model->detachView(&abstractViewMock); }

    void setModuleId(Utils::SmallStringView moduleName, ModuleId moduleId)
    {
        ON_CALL(projectStorageMock, moduleId(Eq(moduleName))).WillByDefault(Return(moduleId));
    }

    void setType(ModuleId moduleId,
                 Utils::SmallStringView typeName,
                 Utils::SmallString defaultPeopertyName)
    {
        static int typeIdNumber = 0;
        TypeId typeId = TypeId::create(++typeIdNumber);

        static int defaultPropertyIdNumber = 0;
        PropertyDeclarationId defaultPropertyId = PropertyDeclarationId::create(
            ++defaultPropertyIdNumber);

        ON_CALL(projectStorageMock, typeId(Eq(moduleId), Eq(typeName), _)).WillByDefault(Return(typeId));
        ON_CALL(projectStorageMock, type(Eq(typeId)))
            .WillByDefault(Return(Info::Type{defaultPropertyId, {}}));
        ON_CALL(projectStorageMock, propertyName(Eq(defaultPropertyId)))
            .WillByDefault(Return(defaultPeopertyName));
    }

    std::vector<ModelNode> nodes() const
    {
        return std::vector<ModelNode>{nodeListProperty.begin(), nodeListProperty.end()};
    }

    static std::vector<ModelNode> nodes(iterator begin, iterator end)
    {
        return std::vector<ModelNode>{begin, end};
    }

protected:
    NiceMock<ProjectStorageMockWithQtQtuick> projectStorageMock;
    std::unique_ptr<QmlDesigner::Model> model{
        std::make_unique<QmlDesigner::Model>(projectStorageMock, "QtQuick.Item")};
    NiceMock<AbstractViewMock> abstractViewMock;
    QmlDesigner::NodeListProperty nodeListProperty;
    ModelNode node1;
    ModelNode node2;
    ModelNode node3;
    ModelNode node4;
    ModelNode node5;
};

TEST_F(NodeListProperty, BeginAndEndItertors)
{
    std::vector<ModelNode> nodes{nodeListProperty.begin(), nodeListProperty.end()};

    ASSERT_THAT(nodes, ElementsAre(node1, node2, node3, node4, node5));
}

TEST_F(NodeListProperty, LoopOverRange)
{
    std::vector<ModelNode> nodes;

    for (const ModelNode &node : nodeListProperty)
        nodes.push_back(node);

    ASSERT_THAT(nodes, ElementsAre(node1, node2, node3, node4, node5));
}

TEST_F(NodeListProperty, NextIterator)
{
    auto begin = nodeListProperty.begin();

    auto nextIterator = std::next(begin);

    ASSERT_THAT(nodes(nextIterator, nodeListProperty.end()), ElementsAre(node2, node3, node4, node5));
}

TEST_F(NodeListProperty, PreviousIterator)
{
    auto end = nodeListProperty.end();

    auto previousIterator = std::prev(end);

    ASSERT_THAT(nodes(nodeListProperty.begin(), previousIterator),
                ElementsAre(node1, node2, node3, node4));
}

TEST_F(NodeListProperty, IncrementIterator)
{
    auto incrementIterator = nodeListProperty.begin();

    ++incrementIterator;

    ASSERT_THAT(nodes(incrementIterator, nodeListProperty.end()),
                ElementsAre(node2, node3, node4, node5));
}

TEST_F(NodeListProperty, IncrementIteratorReturnsIterator)
{
    auto begin = nodeListProperty.begin();

    auto incrementIterator = ++begin;

    ASSERT_THAT(nodes(incrementIterator, nodeListProperty.end()),
                ElementsAre(node2, node3, node4, node5));
}

TEST_F(NodeListProperty, PostIncrementIterator)
{
    auto postIncrementIterator = nodeListProperty.begin();

    postIncrementIterator++;

    ASSERT_THAT(nodes(postIncrementIterator, nodeListProperty.end()),
                ElementsAre(node2, node3, node4, node5));
}

TEST_F(NodeListProperty, PostIncrementIteratorReturnsPreincrementIterator)
{
    auto begin = nodeListProperty.begin();

    auto postIncrementIterator = begin++;

    ASSERT_THAT(nodes(postIncrementIterator, nodeListProperty.end()),
                ElementsAre(node1, node2, node3, node4, node5));
}

TEST_F(NodeListProperty, DecrementIterator)
{
    auto decrementIterator = nodeListProperty.end();

    --decrementIterator;

    ASSERT_THAT(nodes(nodeListProperty.begin(), decrementIterator),
                ElementsAre(node1, node2, node3, node4));
}

TEST_F(NodeListProperty, DerementIteratorReturnsIterator)
{
    auto end = nodeListProperty.end();

    auto decrementIterator = --end;

    ASSERT_THAT(nodes(nodeListProperty.begin(), decrementIterator),
                ElementsAre(node1, node2, node3, node4));
}

TEST_F(NodeListProperty, PostDecrementIterator)
{
    auto postDecrementIterator = nodeListProperty.end();

    postDecrementIterator--;

    ASSERT_THAT(nodes(nodeListProperty.begin(), postDecrementIterator),
                ElementsAre(node1, node2, node3, node4));
}

TEST_F(NodeListProperty, PostDecrementIteratorReturnsPredecrementIterator)
{
    auto end = nodeListProperty.end();

    auto postDecrementIterator = end--;

    ASSERT_THAT(nodes(nodeListProperty.begin(), postDecrementIterator),
                ElementsAre(node1, node2, node3, node4, node5));
}

TEST_F(NodeListProperty, IncrementIteratorByTwo)
{
    auto incrementIterator = nodeListProperty.begin();

    incrementIterator += 2;

    ASSERT_THAT(nodes(incrementIterator, nodeListProperty.end()), ElementsAre(node3, node4, node5));
}

TEST_F(NodeListProperty, IncrementIteratorByTwoReturnsIterator)
{
    auto begin = nodeListProperty.begin();

    auto incrementIterator = begin += 2;

    ASSERT_THAT(nodes(incrementIterator, nodeListProperty.end()), ElementsAre(node3, node4, node5));
}

TEST_F(NodeListProperty, DecrementIteratorByTwo)
{
    auto decrementIterator = nodeListProperty.end();

    decrementIterator -= 2;

    ASSERT_THAT(nodes(nodeListProperty.begin(), decrementIterator), ElementsAre(node1, node2, node3));
}

TEST_F(NodeListProperty, DecrementIteratorByTwoReturnsIterator)
{
    auto end = nodeListProperty.end();

    auto decrementIterator = end -= 2;

    ASSERT_THAT(nodes(nodeListProperty.begin(), decrementIterator), ElementsAre(node1, node2, node3));
}

TEST_F(NodeListProperty, AccessIterator)
{
    auto iterator = std::next(nodeListProperty.begin(), 3);

    auto accessIterator = iterator[2];

    ASSERT_THAT(nodes(accessIterator, nodeListProperty.end()), ElementsAre(node3, node4, node5));
}

TEST_F(NodeListProperty, AddIteratorByIndexSecondOperand)
{
    auto begin = nodeListProperty.begin();

    auto addedIterator = begin + 2;

    ASSERT_THAT(nodes(addedIterator, nodeListProperty.end()), ElementsAre(node3, node4, node5));
}

TEST_F(NodeListProperty, AddIteratorByIndexFirstOperand)
{
    auto begin = nodeListProperty.begin();

    auto addedIterator = 2 + begin;

    ASSERT_THAT(nodes(addedIterator, nodeListProperty.end()), ElementsAre(node3, node4, node5));
}

TEST_F(NodeListProperty, SubtractIterator)
{
    auto end = nodeListProperty.end();

    auto subtractedIterator = end - 3;

    ASSERT_THAT(nodes(subtractedIterator, nodeListProperty.end()), ElementsAre(node3, node4, node5));
}

TEST_F(NodeListProperty, CompareEqualIteratorAreEqual)
{
    auto first = nodeListProperty.begin();
    auto second = nodeListProperty.begin();

    auto isEqual = first == second;

    ASSERT_TRUE(isEqual);
}

TEST_F(NodeListProperty, CompareEqualIteratorAreNotEqual)
{
    auto first = nodeListProperty.begin();
    auto second = std::next(nodeListProperty.begin());

    auto isEqual = first == second;

    ASSERT_FALSE(isEqual);
}

TEST_F(NodeListProperty, CompareUnqualIteratorAreEqual)
{
    auto first = nodeListProperty.begin();
    auto second = nodeListProperty.begin();

    auto isUnequal = first != second;

    ASSERT_FALSE(isUnequal);
}

TEST_F(NodeListProperty, CompareUnequalIteratorAreNotEqual)
{
    auto first = nodeListProperty.begin();
    auto second = std::next(nodeListProperty.begin());

    auto isUnequal = first != second;

    ASSERT_TRUE(isUnequal);
}

TEST_F(NodeListProperty, CompareLessIteratorAreNotLessIfEqual)
{
    auto first = nodeListProperty.begin();
    auto second = nodeListProperty.begin();

    auto isLess = first < second;

    ASSERT_FALSE(isLess);
}

TEST_F(NodeListProperty, CompareLessIteratorAreNotLessIfGreater)
{
    auto first = std::next(nodeListProperty.begin());
    auto second = nodeListProperty.begin();

    auto isLess = first < second;

    ASSERT_FALSE(isLess);
}

TEST_F(NodeListProperty, CompareLessIteratorAreLessIfLess)
{
    auto first = nodeListProperty.begin();
    auto second = std::next(nodeListProperty.begin());

    auto isLess = first < second;

    ASSERT_TRUE(isLess);
}

TEST_F(NodeListProperty, CompareLessEqualIteratorAreLessEqualIfEqual)
{
    auto first = nodeListProperty.begin();
    auto second = nodeListProperty.begin();

    auto isLessEqual = first <= second;

    ASSERT_TRUE(isLessEqual);
}

TEST_F(NodeListProperty, CompareLessEqualIteratorAreNotLessEqualIfGreater)
{
    auto first = std::next(nodeListProperty.begin());
    auto second = nodeListProperty.begin();

    auto isLessEqual = first <= second;

    ASSERT_FALSE(isLessEqual);
}

TEST_F(NodeListProperty, CompareLessEqualIteratorAreLessEqualIfLess)
{
    auto first = nodeListProperty.begin();
    auto second = std::next(nodeListProperty.begin());

    auto isLessEqual = first <= second;

    ASSERT_TRUE(isLessEqual);
}

TEST_F(NodeListProperty, CompareGreaterIteratorAreGreaterIfEqual)
{
    auto first = nodeListProperty.begin();
    auto second = nodeListProperty.begin();

    auto isGreater = first > second;

    ASSERT_FALSE(isGreater);
}

TEST_F(NodeListProperty, CompareGreaterIteratorAreGreaterIfGreater)
{
    auto first = std::next(nodeListProperty.begin());
    auto second = nodeListProperty.begin();

    auto isGreater = first > second;

    ASSERT_TRUE(isGreater);
}

TEST_F(NodeListProperty, CompareGreaterIteratorAreNotGreaterIfLess)
{
    auto first = nodeListProperty.begin();
    auto second = std::next(nodeListProperty.begin());

    auto isGreater = first > second;

    ASSERT_FALSE(isGreater);
}

TEST_F(NodeListProperty, CompareGreaterEqualIteratorAreGreaterEqualIfEqual)
{
    auto first = nodeListProperty.begin();
    auto second = nodeListProperty.begin();

    auto isGreaterEqual = first >= second;

    ASSERT_TRUE(isGreaterEqual);
}

TEST_F(NodeListProperty, CompareGreaterEqualIteratorAreGreaterEqualIfGreater)
{
    auto first = std::next(nodeListProperty.begin());
    auto second = nodeListProperty.begin();

    auto isGreaterEqual = first >= second;

    ASSERT_TRUE(isGreaterEqual);
}

TEST_F(NodeListProperty, CompareGreaterEqualIteratorAreNotGreaterEqualIfLess)
{
    auto first = nodeListProperty.begin();
    auto second = std::next(nodeListProperty.begin());

    auto isGreaterEqual = first >= second;

    ASSERT_FALSE(isGreaterEqual);
}

TEST_F(NodeListProperty, DereferenceIterator)
{
    auto iterator = std::next(nodeListProperty.begin());

    auto node = *iterator;

    ASSERT_THAT(node, Eq(node2));
}

TEST_F(NodeListProperty, IterSwap)
{
    auto first = std::next(nodeListProperty.begin(), 2);
    auto second = nodeListProperty.begin();

    nodeListProperty.iterSwap(first, second);

    ASSERT_THAT(nodes(), ElementsAre(node3, node2, node1, node4, node5));
}

TEST_F(NodeListProperty, Rotate)
{
    auto first = std::next(nodeListProperty.begin());
    auto newFirst = std::next(nodeListProperty.begin(), 2);
    auto last = std::prev(nodeListProperty.end());

    nodeListProperty.rotate(first, newFirst, last);

    ASSERT_THAT(nodes(), ElementsAre(node1, node3, node4, node2, node5));
}

TEST_F(NodeListProperty, RotateCallsNodeOrderedChanged)
{
    auto first = std::next(nodeListProperty.begin());
    auto newFirst = std::next(nodeListProperty.begin(), 2);
    auto last = std::prev(nodeListProperty.end());

    EXPECT_CALL(abstractViewMock, nodeOrderChanged(ElementsAre(node1, node3, node4, node2, node5)));

    nodeListProperty.rotate(first, newFirst, last);
}

TEST_F(NodeListProperty, RotateRange)
{
    auto newFirst = std::prev(nodeListProperty.end(), 2);

    nodeListProperty.rotate(nodeListProperty, newFirst);

    ASSERT_THAT(nodes(), ElementsAre(node4, node5, node1, node2, node3));
}

TEST_F(NodeListProperty, RotateReturnsIterator)
{
    auto first = std::next(nodeListProperty.begin());
    auto newFirst = std::next(nodeListProperty.begin(), 2);
    auto last = std::prev(nodeListProperty.end());

    auto iterator = nodeListProperty.rotate(first, newFirst, last);

    ASSERT_THAT(iterator, Eq(first + (last - newFirst)));
}

TEST_F(NodeListProperty, RotateRangeReturnsIterator)
{
    auto newFirst = std::prev(nodeListProperty.end(), 2);

    auto iterator = nodeListProperty.rotate(nodeListProperty, newFirst);

    ASSERT_THAT(iterator, Eq(nodeListProperty.begin() + (nodeListProperty.end() - newFirst)));
}

TEST_F(NodeListProperty, Reverse)
{
    auto first = std::next(nodeListProperty.begin());
    auto last = std::prev(nodeListProperty.end());

    nodeListProperty.reverse(first, last);

    ASSERT_THAT(nodes(), ElementsAre(node1, node4, node3, node2, node5));
}

TEST_F(NodeListProperty, ReverseCallsNodeOrderedChanged)
{
    auto first = std::next(nodeListProperty.begin());
    auto last = std::prev(nodeListProperty.end());

    EXPECT_CALL(abstractViewMock, nodeOrderChanged(ElementsAre(node1, node4, node3, node2, node5)));

    nodeListProperty.reverse(first, last);
}

} // namespace
