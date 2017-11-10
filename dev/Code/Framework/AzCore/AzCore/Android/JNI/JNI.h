/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#pragma once

#include <AzCore/std/string/string.h>

#include <jni.h>
#include <android/asset_manager.h>


#define HANDLE_JNI_EXCEPTION(jniEnv) \
    jniEnv->ExceptionDescribe(); \
    jniEnv->ExceptionClear();


namespace AZ
{
    namespace Android
    {
        namespace JNI
        {
            //! Request a thread specific JNIEnv pointer from the Android environment.
            //! \return A pointer to the JNIEnv on the current thread.
            JNIEnv* GetEnv();

            //! Loads a Java class as opposed to attempting to find a loaded class from the call stack.
            //! \param classPath The fully qualified forward slash separated Java class path.
            //! \return A global reference to the desired jclass.  Caller is responsible for making a
            //!         call do DeleteGlobalJniRef when the jclass is no longer needed.
            jclass LoadClass(const char* classPath);

            //! Get the fully qualified forward slash separated Java class path of Java class ref.
            //! e.g. android.app.NativeActivity ==> android/app/NativeActivity
            //! \param classRef A valid reference to a java class
            //! \return A copy of the class name
            AZStd::string GetClassName(jclass classRef);

            //! Get just the name of the Java class from a Java class ref.
            //! e.g. android.app.NativeActivity ==> NativeActivity
            //! \param classRef A valid reference to a java class
            //! \return A copy of the class name
            AZStd::string GetSimpleClassName(jclass classRef);

            //! Converts a jstring to a AZStd::string
            //! \param stringValue A local or global reference to a jstring object
            //! \return A copy of the converted string
            AZStd::string ConvertJstringToString(jstring stringValue);

            //! Converts a AZStd::string to a jstring
            //! \param stringValue A local or global reference to a jstring object
            //! \return A global reference to the converted jstring.  The caller is responsible for
            //!         deleting it when no longer needed
            jstring ConvertStringToJstring(const AZStd::string& stringValue);

            //! Gets the reference type of the Java object.  Can be Local, Global or Weak Global.
            //! \param javaRef Raw Java object reference, can be null.
            //! \return The result of GetObjectRefType as long as the object is valid,
            //!         otherwise JNIInvalidRefType.
            int GetRefType(jobject javaRef);

            //! Deletes a JNI object/class reference.  Will handle local, global and weak global references.
            //! \param javaRef Raw java object reference.
            void DeleteRef(jobject javaRef);
        }
    }
}
