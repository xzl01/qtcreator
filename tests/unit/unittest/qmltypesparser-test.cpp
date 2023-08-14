// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "googletest.h"

#include <sqlitedatabase.h>

#include <projectstorage/projectstorage.h>
#include <projectstorage/projectstoragetypes.h>
#include <projectstorage/qmltypesparser.h>
#include <projectstorage/sourcepathcache.h>

namespace {

namespace Storage = QmlDesigner::Storage::Synchronization;
using QmlDesigner::ModuleId;
using QmlDesigner::SourceContextId;
using QmlDesigner::SourceId;

MATCHER_P3(IsImport,
           moduleId,
           version,
           sourceId,
           std::string(negation ? "isn't " : "is ")
               + PrintToString(Storage::Import{moduleId, version, sourceId}))
{
    const Storage::Import &import = arg;

    return import.moduleId == moduleId && import.version == version && import.sourceId == sourceId;
}

MATCHER_P(HasPrototype, prototype, std::string(negation ? "isn't " : "is ") + PrintToString(prototype))
{
    const Storage::Type &type = arg;

    return Storage::ImportedTypeName{prototype} == type.prototype;
}

MATCHER_P5(IsType,
           typeName,
           prototype,
           extension,
           traits,
           sourceId,
           std::string(negation ? "isn't " : "is ")
               + PrintToString(Storage::Type{typeName, prototype, extension, traits, sourceId}))
{
    const Storage::Type &type = arg;

    return type.typeName == typeName && type.prototype == Storage::ImportedTypeName{prototype}
           && type.extension == Storage::ImportedTypeName{extension} && type.traits == traits
           && type.sourceId == sourceId;
}

MATCHER_P3(IsPropertyDeclaration,
           name,
           typeName,
           traits,
           std::string(negation ? "isn't " : "is ")
               + PrintToString(Storage::PropertyDeclaration{name, typeName, traits}))
{
    const Storage::PropertyDeclaration &propertyDeclaration = arg;

    return propertyDeclaration.name == name
           && Storage::ImportedTypeName{typeName} == propertyDeclaration.typeName
           && propertyDeclaration.traits == traits
           && propertyDeclaration.kind == Storage::PropertyKind::Property;
}

MATCHER_P2(IsFunctionDeclaration,
           name,
           returnTypeName,
           std::string(negation ? "isn't " : "is ")
               + PrintToString(Storage::FunctionDeclaration{name, returnTypeName}))
{
    const Storage::FunctionDeclaration &declaration = arg;

    return declaration.name == name && declaration.returnTypeName == returnTypeName;
}

MATCHER_P(IsSignalDeclaration,
          name,
          std::string(negation ? "isn't " : "is ") + PrintToString(Storage::SignalDeclaration{name}))
{
    const Storage::SignalDeclaration &declaration = arg;

    return declaration.name == name;
}

MATCHER_P2(IsParameter,
           name,
           typeName,
           std::string(negation ? "isn't " : "is ")
               + PrintToString(Storage::ParameterDeclaration{name, typeName}))
{
    const Storage::ParameterDeclaration &declaration = arg;

    return declaration.name == name && declaration.typeName == typeName;
}

MATCHER_P(IsEnumeration,
          name,
          std::string(negation ? "isn't " : "is ")
              + PrintToString(Storage::EnumerationDeclaration{name, {}}))
{
    const Storage::EnumerationDeclaration &declaration = arg;

    return declaration.name == name;
}

MATCHER_P(IsEnumerator,
          name,
          std::string(negation ? "isn't " : "is ")
              + PrintToString(Storage::EnumeratorDeclaration{name}))
{
    const Storage::EnumeratorDeclaration &declaration = arg;

    return declaration.name == name && !declaration.hasValue;
}

MATCHER_P2(IsEnumerator,
           name,
           value,
           std::string(negation ? "isn't " : "is ")
               + PrintToString(Storage::EnumeratorDeclaration{name, value, true}))
{
    const Storage::EnumeratorDeclaration &declaration = arg;

    return declaration.name == name && declaration.value == value && declaration.hasValue;
}

MATCHER_P3(IsExportedType,
           moduleId,
           name,
           version,
           std::string(negation ? "isn't " : "is ")
               + PrintToString(Storage::ExportedType{moduleId, name, version}))
{
    const Storage::ExportedType &type = arg;

    return type.name == name && type.moduleId == moduleId && type.version == version;
}

class QmlTypesParser : public ::testing::Test
{
public:
protected:
    Sqlite::Database database{":memory:", Sqlite::JournalMode::Memory};
    QmlDesigner::ProjectStorage<Sqlite::Database> storage{database, database.isInitialized()};
    QmlDesigner::SourcePathCache<QmlDesigner::ProjectStorage<Sqlite::Database>> sourcePathCache{
        storage};
    QmlDesigner::QmlTypesParser parser{storage};
    Storage::Imports imports;
    Storage::Types types;
    SourceId qmltypesFileSourceId{sourcePathCache.sourceId("path/to/types.qmltypes")};
    ModuleId qtQmlNativeModuleId = storage.moduleId("QtQml-cppnative");
    Storage::ProjectData projectData{qmltypesFileSourceId,
                                     qmltypesFileSourceId,
                                     qtQmlNativeModuleId,
                                     Storage::FileType::QmlTypes};
    SourceContextId qmltypesFileSourceContextId{sourcePathCache.sourceContextId(qmltypesFileSourceId)};
    ModuleId directoryModuleId{storage.moduleId("path/to/")};
};

TEST_F(QmlTypesParser, Imports)
{
    QString source{R"(import QtQuick.tooling 1.2
                      Module{
                        dependencies:
                          ["QtQuick 2.15", "QtQuick.Window 2.1", "QtFoo 6"]})"};

    parser.parse(source, imports, types, projectData);

    ASSERT_THAT(imports,
                UnorderedElementsAre(IsImport(storage.moduleId("QML-cppnative"),
                                              QmlDesigner::Storage::Version{},
                                              qmltypesFileSourceId),
                                     IsImport(storage.moduleId("QtQml-cppnative"),
                                              QmlDesigner::Storage::Version{},
                                              qmltypesFileSourceId),
                                     IsImport(storage.moduleId("QtQuick-cppnative"),
                                              QmlDesigner::Storage::Version{},
                                              qmltypesFileSourceId),
                                     IsImport(storage.moduleId("QtQuick.Window-cppnative"),
                                              QmlDesigner::Storage::Version{},
                                              qmltypesFileSourceId),
                                     IsImport(storage.moduleId("QtFoo-cppnative"),
                                              QmlDesigner::Storage::Version{},
                                              qmltypesFileSourceId)));
}

TEST_F(QmlTypesParser, Types)
{
    QString source{R"(import QtQuick.tooling 1.2
                      Module{
                        Component { name: "QObject"}
                        Component { name: "QQmlComponent"}})"};

    parser.parse(source, imports, types, projectData);

    ASSERT_THAT(types,
                UnorderedElementsAre(IsType("QObject",
                                            Storage::ImportedType{},
                                            Storage::ImportedType{},
                                            QmlDesigner::Storage::TypeTraits::Reference,
                                            qmltypesFileSourceId),
                                     IsType("QQmlComponent",
                                            Storage::ImportedType{},
                                            Storage::ImportedType{},
                                            QmlDesigner::Storage::TypeTraits::Reference,
                                            qmltypesFileSourceId)));
}

TEST_F(QmlTypesParser, Prototype)
{
    QString source{R"(import QtQuick.tooling 1.2
                      Module{
                        Component { name: "QObject"}
                        Component { name: "QQmlComponent"
                                    prototype: "QObject"}})"};

    parser.parse(source, imports, types, projectData);

    ASSERT_THAT(types,
                UnorderedElementsAre(IsType("QObject",
                                            Storage::ImportedType{},
                                            Storage::ImportedType{},
                                            QmlDesigner::Storage::TypeTraits::Reference,
                                            qmltypesFileSourceId),
                                     IsType("QQmlComponent",
                                            Storage::ImportedType{"QObject"},
                                            Storage::ImportedType{},
                                            QmlDesigner::Storage::TypeTraits::Reference,
                                            qmltypesFileSourceId)));
}

TEST_F(QmlTypesParser, Extension)
{
    QString source{R"(import QtQuick.tooling 1.2
                      Module{
                        Component { name: "QObject"}
                        Component { name: "QQmlComponent"
                                    extension: "QObject"}})"};

    parser.parse(source, imports, types, projectData);

    ASSERT_THAT(types,
                UnorderedElementsAre(IsType("QObject",
                                            Storage::ImportedType{},
                                            Storage::ImportedType{},
                                            QmlDesigner::Storage::TypeTraits::Reference,
                                            qmltypesFileSourceId),
                                     IsType("QQmlComponent",
                                            Storage::ImportedType{},
                                            Storage::ImportedType{"QObject"},
                                            QmlDesigner::Storage::TypeTraits::Reference,
                                            qmltypesFileSourceId)));
}

TEST_F(QmlTypesParser, ExportedTypes)
{
    QString source{R"(import QtQuick.tooling 1.2
                      Module{
                        Component { name: "QObject"
                          exports: ["QML/QtObject 1.0", "QtQml/QtObject 2.1"]
                      }})"};
    ModuleId qmlModuleId = storage.moduleId("QML");
    ModuleId qtQmlModuleId = storage.moduleId("QtQml");

    parser.parse(source, imports, types, projectData);

    ASSERT_THAT(
        types,
        ElementsAre(Field(
            &Storage::Type::exportedTypes,
            UnorderedElementsAre(
                IsExportedType(qmlModuleId, "QtObject", QmlDesigner::Storage::Version{1, 0}),
                IsExportedType(qtQmlModuleId, "QtObject", QmlDesigner::Storage::Version{2, 1}),
                IsExportedType(qtQmlNativeModuleId, "QObject", QmlDesigner::Storage::Version{})))));
}

TEST_F(QmlTypesParser, Properties)
{
    QString source{R"(import QtQuick.tooling 1.2
                      Module{
                        Component { name: "QObject"
                          Property { name: "objectName"; type: "string" }
                          Property { name: "target"; type: "QObject"; isPointer: true }
                          Property { name: "progress"; type: "double"; isReadonly: true }
                          Property { name: "targets"; type: "QQuickItem"; isList: true; isReadonly: true; isPointer: true }
                      }})"};

    parser.parse(source, imports, types, projectData);

    ASSERT_THAT(
        types,
        ElementsAre(Field(
            &Storage::Type::propertyDeclarations,
            UnorderedElementsAre(
                IsPropertyDeclaration("objectName",
                                      Storage::ImportedType{"string"},
                                      QmlDesigner::Storage::PropertyDeclarationTraits::None),
                IsPropertyDeclaration("target",
                                      Storage::ImportedType{"QObject"},
                                      QmlDesigner::Storage::PropertyDeclarationTraits::IsPointer),
                IsPropertyDeclaration("progress",
                                      Storage::ImportedType{"double"},
                                      QmlDesigner::Storage::PropertyDeclarationTraits::IsReadOnly),
                IsPropertyDeclaration("targets",
                                      Storage::ImportedType{"QQuickItem"},
                                      QmlDesigner::Storage::PropertyDeclarationTraits::IsReadOnly
                                          | QmlDesigner::Storage::PropertyDeclarationTraits::IsList
                                          | QmlDesigner::Storage::PropertyDeclarationTraits::IsPointer)))));
}

TEST_F(QmlTypesParser, PropertiesWithQualifiedTypes)
{
    QString source{R"(import QtQuick.tooling 1.2
                      Module{
                        Component { name: "Qt::Vector" }
                        Component { name: "Qt::List" }
                        Component { name: "QObject"
                          Property { name: "values"; type: "Vector" }
                          Property { name: "items"; type: "List" }
                          Property { name: "values2"; type: "Qt::Vector" }
                      }})"};

    parser.parse(source, imports, types, projectData);

    ASSERT_THAT(
        types,
        Contains(
            Field(&Storage::Type::propertyDeclarations,
                  UnorderedElementsAre(
                      IsPropertyDeclaration("values",
                                            Storage::ImportedType{"Qt::Vector"},
                                            QmlDesigner::Storage::PropertyDeclarationTraits::None),
                      IsPropertyDeclaration("items",
                                            Storage::ImportedType{"Qt::List"},
                                            QmlDesigner::Storage::PropertyDeclarationTraits::None),
                      IsPropertyDeclaration("values2",
                                            Storage::ImportedType{"Qt::Vector"},
                                            QmlDesigner::Storage::PropertyDeclarationTraits::None)))));
}

TEST_F(QmlTypesParser, PropertiesWithoutType)
{
    QString source{R"(import QtQuick.tooling 1.2
                      Module{
                        Component { name: "QObject"
                          Property { name: "objectName"}
                          Property { name: "target"; type: "QObject"; isPointer: true }
                      }})"};

    parser.parse(source, imports, types, projectData);

    ASSERT_THAT(types,
                ElementsAre(Field(&Storage::Type::propertyDeclarations,
                                  UnorderedElementsAre(IsPropertyDeclaration(
                                      "target",
                                      Storage::ImportedType{"QObject"},
                                      QmlDesigner::Storage::PropertyDeclarationTraits::IsPointer)))));
}

TEST_F(QmlTypesParser, Functions)
{
    QString source{R"(import QtQuick.tooling 1.2
                      Module{
                        Component { name: "QObject"
                          Method { name: "movieUpdate" }
                          Method {
                            name: "advance"
                            Parameter { name: "frames"; type: "int" }
                            Parameter { name: "fps"; type: "double" }
                          }
                          Method {
                            name: "isImageLoading"
                            type: "bool"
                            Parameter { name: "url"; type: "QUrl" }
                          }
                          Method {
                            name: "getContext"
                            Parameter { name: "args"; type: "QQmlV4Function"; isPointer: true }
                          }
                      }})"};

    parser.parse(source, imports, types, projectData);

    ASSERT_THAT(types,
                ElementsAre(
                    Field(&Storage::Type::functionDeclarations,
                          UnorderedElementsAre(
                              AllOf(IsFunctionDeclaration("advance", ""),
                                    Field(&Storage::FunctionDeclaration::parameters,
                                          UnorderedElementsAre(IsParameter("frames", "int"),
                                                               IsParameter("fps", "double")))),
                              AllOf(IsFunctionDeclaration("isImageLoading", "bool"),
                                    Field(&Storage::FunctionDeclaration::parameters,
                                          UnorderedElementsAre(IsParameter("url", "QUrl")))),
                              AllOf(IsFunctionDeclaration("getContext", ""),
                                    Field(&Storage::FunctionDeclaration::parameters,
                                          UnorderedElementsAre(IsParameter("args", "QQmlV4Function")))),
                              AllOf(IsFunctionDeclaration("movieUpdate", ""),
                                    Field(&Storage::FunctionDeclaration::parameters, IsEmpty()))))));
}

TEST_F(QmlTypesParser, SkipJavaScriptFunctions)
{
    QString source{R"(import QtQuick.tooling 1.2
                      Module{
                        Component { name: "QObject"
                          Method {
                            name: "do"
                            isJavaScriptFunction: true
                          }
                      }})"};

    parser.parse(source, imports, types, projectData);

    ASSERT_THAT(types, ElementsAre(Field(&Storage::Type::functionDeclarations, IsEmpty())));
}

TEST_F(QmlTypesParser, FunctionsWithQualifiedTypes)
{
    QString source{R"(import QtQuick.tooling 1.2
                      Module{
                        Component { name: "Qt::Vector" }
                        Component { name: "Qt::List" }
                        Component { name: "QObject"
                          Method {
                            name: "values"
                            Parameter { name: "values"; type: "Vector" }
                            Parameter { name: "items"; type: "List" }
                            Parameter { name: "values2"; type: "Qt::Vector" }
                          }
                      }})"};

    parser.parse(source, imports, types, projectData);

    ASSERT_THAT(types,
                Contains(
                    Field(&Storage::Type::functionDeclarations,
                          UnorderedElementsAre(AllOf(
                              IsFunctionDeclaration("values", ""),
                              Field(&Storage::FunctionDeclaration::parameters,
                                    UnorderedElementsAre(IsParameter("values", "Qt::Vector"),
                                                         IsParameter("items", "Qt::List"),
                                                         IsParameter("values2", "Qt::Vector"))))))));
}

TEST_F(QmlTypesParser, Signals)
{
    QString source{R"(import QtQuick.tooling 1.2
                      Module{
                        Component { name: "QObject"
                          Method { name: "movieUpdate" }
                          Signal {
                            name: "advance"
                            Parameter { name: "frames"; type: "int" }
                            Parameter { name: "fps"; type: "double" }
                          }
                          Signal {
                            name: "isImageLoading"
                            Parameter { name: "url"; type: "QUrl" }
                          }
                          Signal {
                            name: "getContext"
                            Parameter { name: "args"; type: "QQmlV4Function"; isPointer: true }
                          }
                      }})"};

    parser.parse(source, imports, types, projectData);

    ASSERT_THAT(types,
                ElementsAre(Field(&Storage::Type::signalDeclarations,
                                  UnorderedElementsAre(
                                      AllOf(IsSignalDeclaration("advance"),
                                            Field(&Storage::SignalDeclaration::parameters,
                                                  UnorderedElementsAre(IsParameter("frames", "int"),
                                                                       IsParameter("fps", "double")))),
                                      AllOf(IsSignalDeclaration("isImageLoading"),
                                            Field(&Storage::SignalDeclaration::parameters,
                                                  UnorderedElementsAre(IsParameter("url", "QUrl")))),
                                      AllOf(IsSignalDeclaration("getContext"),
                                            Field(&Storage::SignalDeclaration::parameters,
                                                  UnorderedElementsAre(
                                                      IsParameter("args", "QQmlV4Function"))))))));
}

TEST_F(QmlTypesParser, SignalsWithQualifiedTypes)
{
    QString source{R"(import QtQuick.tooling 1.2
                      Module{
                        Component { name: "Qt::Vector" }
                        Component { name: "Qt::List" }
                        Component { name: "QObject"
                          Signal {
                            name: "values"
                            Parameter { name: "values"; type: "Vector" }
                            Parameter { name: "items"; type: "List" }
                            Parameter { name: "values2"; type: "Qt::Vector" }
                          }
                      }})"};

    parser.parse(source, imports, types, projectData);

    ASSERT_THAT(types,
                Contains(
                    Field(&Storage::Type::signalDeclarations,
                          UnorderedElementsAre(AllOf(
                              IsSignalDeclaration("values"),
                              Field(&Storage::SignalDeclaration::parameters,
                                    UnorderedElementsAre(IsParameter("values", "Qt::Vector"),
                                                         IsParameter("items", "Qt::List"),
                                                         IsParameter("values2", "Qt::Vector"))))))));
}

TEST_F(QmlTypesParser, Enumerations)
{
    QString source{R"(import QtQuick.tooling 1.2
                      Module{
                        Component { name: "QObject"
                          Enum {
                              name: "NamedColorSpace"
                              values: [
                                  "Unknown",
                                  "SRgb",
                                  "AdobeRgb",
                                  "DisplayP3",
                              ]
                          }
                          Enum {
                              name: "VerticalLayoutDirection"
                              values: ["TopToBottom", "BottomToTop"]
                          }
                      }})"};

    parser.parse(source, imports, types, projectData);

    ASSERT_THAT(types,
                Contains(
                    Field(&Storage::Type::enumerationDeclarations,
                          UnorderedElementsAre(
                              AllOf(IsEnumeration("NamedColorSpace"),
                                    Field(&Storage::EnumerationDeclaration::enumeratorDeclarations,
                                          UnorderedElementsAre(IsEnumerator("Unknown"),
                                                               IsEnumerator("SRgb"),
                                                               IsEnumerator("AdobeRgb"),
                                                               IsEnumerator("DisplayP3")))),
                              AllOf(IsEnumeration("VerticalLayoutDirection"),
                                    Field(&Storage::EnumerationDeclaration::enumeratorDeclarations,
                                          UnorderedElementsAre(IsEnumerator("TopToBottom"),
                                                               IsEnumerator("BottomToTop"))))))));
}

TEST_F(QmlTypesParser, EnumerationIsExportedAsType)
{
    QString source{R"(import QtQuick.tooling 1.2
                      Module{
                        Component { name: "QObject"
                          Enum {
                              name: "NamedColorSpace"
                              values: [
                                  "Unknown",
                                  "SRgb",
                                  "AdobeRgb",
                                  "DisplayP3",
                              ]
                          }
                          Enum {
                              name: "VerticalLayoutDirection"
                              values: ["TopToBottom", "BottomToTop"]
                          }
                          exports: ["QML/QtObject 1.0", "QtQml/QtObject 2.1"]
                      }})"};

    parser.parse(source, imports, types, projectData);

    ASSERT_THAT(
        types,
        UnorderedElementsAre(
            AllOf(IsType("QObject::NamedColorSpace",
                         Storage::ImportedType{},
                         Storage::ImportedType{},
                         QmlDesigner::Storage::TypeTraits::Value | QmlDesigner::Storage::TypeTraits::IsEnum,
                         qmltypesFileSourceId),
                  Field(&Storage::Type::exportedTypes,
                        UnorderedElementsAre(IsExportedType(qtQmlNativeModuleId,
                                                            "QObject::NamedColorSpace",
                                                            QmlDesigner::Storage::Version{})))),
            AllOf(IsType("QObject::VerticalLayoutDirection",
                         Storage::ImportedType{},
                         Storage::ImportedType{},
                         QmlDesigner::Storage::TypeTraits::Value | QmlDesigner::Storage::TypeTraits::IsEnum,
                         qmltypesFileSourceId),
                  Field(&Storage::Type::exportedTypes,
                        UnorderedElementsAre(IsExportedType(qtQmlNativeModuleId,
                                                            "QObject::VerticalLayoutDirection",
                                                            QmlDesigner::Storage::Version{})))),
            _));
}

TEST_F(QmlTypesParser, EnumerationIsExportedAsTypeWithAlias)
{
    QString source{R"(import QtQuick.tooling 1.2
                      Module{
                        Component { name: "QObject"
                          Enum {
                              name: "NamedColorSpaces"
                              alias: "NamedColorSpace"
                              values: [
                                  "Unknown",
                                  "SRgb",
                                  "AdobeRgb",
                                  "DisplayP3",
                              ]
                          }
                          exports: ["QML/QtObject 1.0", "QtQml/QtObject 2.1"]
                      }})"};

    parser.parse(source, imports, types, projectData);

    ASSERT_THAT(types,
                UnorderedElementsAre(
                    AllOf(IsType("QObject::NamedColorSpaces",
                                 Storage::ImportedType{},
                                 Storage::ImportedType{},
                                 QmlDesigner::Storage::TypeTraits::Value
                                     | QmlDesigner::Storage::TypeTraits::IsEnum,
                                 qmltypesFileSourceId),
                          Field(&Storage::Type::exportedTypes,
                                UnorderedElementsAre(IsExportedType(qtQmlNativeModuleId,
                                                                    "QObject::NamedColorSpace",
                                                                    QmlDesigner::Storage::Version{}),
                                                     IsExportedType(qtQmlNativeModuleId,
                                                                    "QObject::NamedColorSpaces",
                                                                    QmlDesigner::Storage::Version{})))),
                    _));
}

TEST_F(QmlTypesParser, EnumerationIsExportedAsTypeWithAliasToo)
{
    QString source{R"(import QtQuick.tooling 1.2
                      Module{
                        Component { name: "QObject"
                          Enum {
                              name: "NamedColorSpaces"
                              alias: "NamedColorSpace"
                              values: [
                                  "Unknown",
                                  "SRgb",
                                  "AdobeRgb",
                                  "DisplayP3",
                              ]
                          }
                          Enum {
                              name: "NamedColorSpace"
                              values: [
                                  "Unknown",
                                  "SRgb",
                                  "AdobeRgb",
                                  "DisplayP3",
                              ]
                          }
                          exports: ["QML/QtObject 1.0", "QtQml/QtObject 2.1"]
                      }})"};

    parser.parse(source, imports, types, projectData);

    ASSERT_THAT(types,
                UnorderedElementsAre(
                    AllOf(IsType("QObject::NamedColorSpaces",
                                 Storage::ImportedType{},
                                 Storage::ImportedType{},
                                 QmlDesigner::Storage::TypeTraits::Value
                                     | QmlDesigner::Storage::TypeTraits::IsEnum,
                                 qmltypesFileSourceId),
                          Field(&Storage::Type::exportedTypes,
                                UnorderedElementsAre(IsExportedType(qtQmlNativeModuleId,
                                                                    "QObject::NamedColorSpace",
                                                                    QmlDesigner::Storage::Version{}),
                                                     IsExportedType(qtQmlNativeModuleId,
                                                                    "QObject::NamedColorSpaces",
                                                                    QmlDesigner::Storage::Version{})))),
                    _));
}

TEST_F(QmlTypesParser, EnumerationIsReferencedByQualifiedName)
{
    QString source{R"(import QtQuick.tooling 1.2
                      Module{
                        Component { name: "QObject"
                          Property { name: "colorSpace"; type: "NamedColorSpace" }
                          Enum {
                              name: "NamedColorSpace"
                              values: [
                                  "Unknown",
                                  "SRgb",
                                  "AdobeRgb",
                                  "DisplayP3",
                              ]
                          }
                      }})"};

    parser.parse(source, imports, types, projectData);

    ASSERT_THAT(types,
                Contains(Field(&Storage::Type::propertyDeclarations,
                               ElementsAre(IsPropertyDeclaration(
                                   "colorSpace",
                                   Storage::ImportedType{"QObject::NamedColorSpace"},
                                   QmlDesigner::Storage::PropertyDeclarationTraits::None)))));
}

TEST_F(QmlTypesParser, AliasEnumerationIsReferencedByQualifiedName)
{
    QString source{R"(import QtQuick.tooling 1.2
                      Module{
                        Component { name: "QObject"
                          Property { name: "colorSpace"; type: "NamedColorSpaces" }
                          Enum {
                              name: "NamedColorSpace"
                              alias: "NamedColorSpaces"
                              values: [
                                  "Unknown",
                                  "SRgb",
                                  "AdobeRgb",
                                  "DisplayP3",
                              ]
                          }
                      }})"};

    parser.parse(source, imports, types, projectData);

    ASSERT_THAT(types,
                Contains(Field(&Storage::Type::propertyDeclarations,
                               ElementsAre(IsPropertyDeclaration(
                                   "colorSpace",
                                   Storage::ImportedType{"QObject::NamedColorSpaces"},
                                   QmlDesigner::Storage::PropertyDeclarationTraits::None)))));
}

} // namespace
