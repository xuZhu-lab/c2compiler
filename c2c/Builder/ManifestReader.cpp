/* Copyright 2013-2020 Bas van den Berg
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Builder/ManifestReader.h"
#include "Builder/Manifest.h"
#include "FileUtils/TomlReader.h"
#include "AST/Component.h"

using namespace C2;

// TODO move to Component
static Component::Type str2dep(const char* type) {
    if (strcmp("static", type) == 0)  return Component::STATIC_LIB;
    if (strcmp("dynamic", type) == 0) return Component::SHARED_LIB;
    if (strcmp("source", type) == 0)  return Component::SOURCE_LIB;
    return (Component::Type)-1;
}

bool ManifestReader::parse()
{
    errorMsg[0] = 0;
    TomlReader reader;
    if (!reader.parse(manifest.filename.c_str())) return false;

    const char* lang = reader.getValue("library.language");
    if (!lang) {
        sprintf(errorMsg, "missing library.language");
        return false;
    }
    manifest.isNative = (strcmp(lang, "C2") == 0);

    // optional
    const char* linkname = reader.getValue("library.linkname");
    if (linkname) manifest.linkName = linkname;

    // required
    TomlReader::ValueIter typeIter = reader.getValueIter("library.type");
    while (!typeIter.done()) {
        const char* typeStr = typeIter.getValue();
        Component::Type type = str2dep(typeStr);
        if ((int)type == -1) {
            sprintf(errorMsg, "invalid dep type '%s'", typeStr);
            return false;
        }
        switch (type) {
        case Component::EXECUTABLE:
            break;
        case Component::SHARED_LIB:
            manifest.hasDynamicLib = true;
            break;
        case Component::STATIC_LIB:
            manifest.hasStaticLib = true;
            break;
        case Component::SOURCE_LIB:
            if (!manifest.isNative) {
                sprintf(errorMsg, "language must be C2 for source libraries");
                return false;
            }
            if (manifest.hasDynamicLib || manifest.hasStaticLib) {
                sprintf(errorMsg, "source library type cannot be combined with shared or static one");
                return false;
            }
            manifest.hasSourceLib = true;
            break;
        }
        typeIter.next();
    }

    TomlReader::NodeIter depsIter = reader.getNodeIter("deps");
    while (!depsIter.done()) {
        const char* depName = depsIter.getValue("name");
        const char* depType = depsIter.getValue("type");
        if (!depName || !depType) {
            sprintf(errorMsg, "dependency needs name and type");
            return false;
        }
        Component::Type type = str2dep(depType);
        if ((int)type == -1) {
            sprintf(errorMsg, "invalid dep type '%s'", depType);
            return false;
        }
        for (unsigned i=0; i<manifest.deps.size(); i++) {
            if (manifest.deps[i] == depName) {
                sprintf(errorMsg, "duplicate dependency on '%s'", depName);
                return false;
            }
        }
        // TODO also store/use deps type
        manifest.deps.push_back(depName);
        depsIter.next();
    }

    TomlReader::NodeIter moduleIter = reader.getNodeIter("module");
    while (!moduleIter.done()) {
        const char* name = moduleIter.getValue("name");
        if (!name) {
            sprintf(errorMsg, "missing module name");
            return false;
        }
        manifest.modules.push_back(name);
        moduleIter.next();
    }
    return true;
}

