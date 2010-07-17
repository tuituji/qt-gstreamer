/*
    Copyright (C) 2010  George Kiagiadakis <kiagiadakis.george@gmail.com>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "generator.h"
#include <cstdio>
#include <cstdlib>
#include <QtCore/QCoreApplication>
#include <QtCore/QFile>

extern int yylineno;
int yyparse(CodeGen *codegen);
void yyrestart(FILE *file);

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QTextStream outStream(stdout);
    outStream << "// Autogenerated by the QtGstreamer helper code generator" << endl
              << "#include <boost/static_assert.hpp>" << endl;

    for (int i=1; i<argc; ++i) {
        QString fileName(QFile::decodeName(argv[i]));
        if (fileName.startsWith("-I")) {
            outStream << "#include <" << fileName.remove(0, 2) << ">" << endl;
        } else if (fileName.endsWith(".h") && QFile::exists(fileName)) {
            CodeGen::parse(fileName);
        } else {
            QTextStream(stderr) << "Skipping " << fileName << ": Not an existing header" << endl;
        }
    }

    return 0;
}

void CodeGen::parse(const QString & fileName)
{
    CodeGen codegen(fileName);

    FILE *fp = std::fopen(QFile::encodeName(fileName), "r");
    if (!fp) {
        std::perror("fopen");
        QTextStream(stderr) << "Could not open " << fileName << endl;
        return;
    }

    yylineno = 1;
    yyrestart(fp);
    yyparse(&codegen);
    std::fclose(fp);

    codegen.generateOutput();
}

void CodeGen::generateOutput()
{
    QTextStream outStream(stdout);
    outStream << "#include \"" << m_fileName << "\"" << endl << endl;

    foreach(const TypeRegistration & typeReg, m_typeRegistrations) {
        printTypeRegistration(outStream, typeReg);
        outStream << endl;
    }

    foreach(const Enum & enumDef, m_enums) {
        if (!enumDef.options.contains("skip")) {
            printEnumAssertions(outStream, enumDef);
        }
        outStream << endl;
    }
}

void CodeGen::printTypeRegistration(QTextStream & outStream, const TypeRegistration & typeReg)
{
    outStream << "QGLIB_REGISTER_TYPE_IMPLEMENTATION(";

    outStream << typeReg["namespace"] << "::" << typeReg["class"];
    if (!typeReg["enum"].isEmpty()) {
        outStream << "::" << typeReg["enum"];
    }

    outStream << ',';

    if (typeReg.contains("GType")) {
        outStream << typeReg["GType"];
    } else {
        outStream << (typeReg["namespace"] == "QGst" ? "GST_TYPE_" : "G_TYPE_");
        outStream << toGstStyle(typeReg["enum"].isEmpty() ? typeReg["class"] : typeReg["enum"]);
    }
    outStream << ")" << endl;
}

void CodeGen::printEnumAssertions(QTextStream& outStream, const Enum & enumDef)
{
    outStream << "namespace " << enumDef.options["namespace"] << " {" << endl;

    foreach(const QByteArray & value, enumDef.values) {
        outStream << "    BOOST_STATIC_ASSERT(static_cast<int>(";
        if (enumDef.options.contains("class") && !enumDef.options["class"].isEmpty()) {
            outStream << enumDef.options["class"] << "::";
        }
        outStream << value;
        outStream << ") == static_cast<int>(";

        if (enumDef.options.contains("prefix")) {
            outStream << enumDef.options["prefix"];
        } else {
            outStream << (enumDef.options["namespace"] == "QGst" ? "GST_" : "G_");
            if (enumDef.options.contains("class") && !enumDef.options["class"].isEmpty()) {
                outStream << toGstStyle(enumDef.options["class"]) << "_";
            }
        }

        if (enumDef.options.contains(value)) {
            outStream << enumDef.options[value];
        } else {
            outStream << toGstStyle(value);
        }
        outStream << "));" << endl;
    }
    outStream << "}" << endl;
}

QByteArray CodeGen::toGstStyle(const QByteArray & str)
{
    QByteArray output;
    foreach(const char currentChar, str) {
        if (isupper(currentChar)) {
            if (!output.isEmpty()) { //if this is not the first char
                output.append('_');
            }
            output.append(currentChar);
        } else {
            output.append(toupper(currentChar));
        }
    }
    return output;
}


void CodeGen::addEnum(const QList<QByteArray> & values, const QHash<QByteArray, QByteArray> & options)
{
    Enum e;
    e.values = values;
    e.options = options;
    e.options["namespace"] = m_currentNamespace;
    e.options["class"] = m_currentClass;
    m_enums.append(e);
}

void CodeGen::addTypeRegistration(const QByteArray& namespaceId, const QByteArray& classId,
                                 const QByteArray& enumId, const QHash<QByteArray, QByteArray>& options)
{
    TypeRegistration typeReg(options);
    typeReg["namespace"] = namespaceId;
    typeReg["class"] = classId;
    typeReg["enum"] = enumId;
    m_typeRegistrations.append(typeReg);
}

//called by yyerror()
void CodeGen::fatalError(const char* msg)
{
    QTextStream(stderr) << "codegen: " << m_fileName << ":" << yylineno << ": error: " << msg << endl;
    std::exit(1);
}

