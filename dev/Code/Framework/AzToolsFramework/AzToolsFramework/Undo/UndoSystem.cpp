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
#include "stdafx.h"

#include "UndoSystem.h"

namespace AzToolsFramework
{
    namespace UndoSystem
    {
        URSequencePoint::URSequencePoint(const AZStd::string& friendlyName, URCommandID ID)
        {
            m_isPosted = false;
            m_Parent = NULL;
            m_FriendlyName = friendlyName;
            m_ID = ID;

            //AZ_TracePrintf("Undo System", "New Root Point %d\n",ID);
        }
        URSequencePoint::URSequencePoint(URCommandID ID)
        {
            m_isPosted = false;
            m_Parent = NULL;
            m_ID = ID;
            m_FriendlyName = AZStd::string("Unknown Undo Command");
        }
        URSequencePoint::~URSequencePoint()
        {
            for (ChildVec::iterator it = m_Children.begin(); it != m_Children.end(); ++it)
            {
                URSequencePoint* childPtr = *it;
                delete childPtr;
            }
            m_Children.clear();
        }

        void URSequencePoint::RunUndo()
        {
            // reversed children, then me
            for (ChildVec::reverse_iterator it = m_Children.rbegin(); it != m_Children.rend(); ++it)
            {
                (*it)->RunUndo();
            }

            Undo();
        }
        void URSequencePoint::RunRedo()
        {
            // me, then children forward
            Redo();

            for (ChildVec::iterator it = m_Children.begin(); it != m_Children.end(); ++it)
            {
                (*it)->RunRedo();
            }
        }

        void URSequencePoint::Undo()
        {
        }

        void URSequencePoint::Redo()
        {
        }

        URSequencePoint* URSequencePoint::Find(URCommandID ID, const AZ::Uuid& typeOfCommand)
        {
            if ((*this == ID) && (this->RTTI_IsTypeOf(typeOfCommand)))
            {
                return this;
            }

            for (ChildVec::iterator it = m_Children.begin(); it != m_Children.end(); ++it)
            {
                URSequencePoint* cmd = (*it)->Find(ID, typeOfCommand);
                if (cmd)
                {
                    return cmd;
                }
            }

            return NULL;
        }

        // does it have children objects that do anything?
        bool URSequencePoint::HasRealChildren() const
        {
            if (m_Children.empty())
            {
                return false;
            }

            for (auto it = m_Children.begin(); it != m_Children.end(); ++it)
            {
                URSequencePoint* pChild = *it;
                if ((pChild->RTTI_GetType() != AZ::AzTypeInfo<URSequencePoint>::Uuid()) || (pChild->HasRealChildren()))
                {
                    return true;
                }
            }
            return false;
        }

        void URSequencePoint::SetParent(URSequencePoint* parent)
        {
            if (m_Parent != nullptr)
            {
                m_Parent->RemoveChild(this);
            }

            m_Parent = parent;
            m_Parent->AddChild(this);
        }

        void URSequencePoint::SetName(const AZStd::string& friendlyName)
        {
            m_FriendlyName = friendlyName;
        }
        void URSequencePoint::AddChild(URSequencePoint* child)
        {
            m_Children.push_back(child);
        }

        void URSequencePoint::RemoveChild(URSequencePoint* child)
        {
            auto it = AZStd::find(m_Children.begin(), m_Children.end(), child);
            if (it != m_Children.end())
            {
                m_Children.erase(it);
            }
        }

        AZStd::string& URSequencePoint::GetName()
        {
            return m_FriendlyName;
        }

        void URSequencePoint::ApplyToTree(const ApplyOperationCB& applyCB)
        {
            applyCB(this);

            for (ChildVec::iterator it = m_Children.begin(); it != m_Children.end(); ++it)
            {
                (*it)->ApplyToTree(applyCB);
            }
        }

        //--------------------------------------------------------------------
        UndoStack::UndoStack(int limit, IUndoNotify* notify)
            : m_SequencePointsBuffer(limit)
        {
            m_notify = notify;
            reentryGuard = false;
            m_Cursor = m_CleanPoint = -1;
#ifdef _DEBUG
            CleanCheck();
#endif
        }
        UndoStack::~UndoStack()
        {
            for (int idx = 0; idx < int(m_SequencePointsBuffer.size()); ++idx)
            {
                delete m_SequencePointsBuffer[idx];
                m_SequencePointsBuffer[idx] = NULL;
            }
        }

        URSequencePoint* UndoStack::Post(URSequencePoint* cmd)
        {
            //AZ_TracePrintf("Undo System", "New command\n");
            AZ_Assert(cmd, "UndoStack Post( NULL )");
            AZ_Assert(cmd->GetParent() == nullptr, "You may not add undo commands with parents to the undo stack.");
            AZ_Assert(!cmd->IsPosted(), "The given command is posted to the Undo Queue already");
            cmd->m_isPosted = true;

            // this is a new command at the cursor
            // any commands beyond the cursor are invalidated thereby
            Slice();

            if (m_SequencePointsBuffer.full())
            {
                delete m_SequencePointsBuffer[0];
                m_SequencePointsBuffer[0] = NULL;

                m_SequencePointsBuffer.pop_front();
                --m_CleanPoint;
            }
            m_SequencePointsBuffer.push_back(cmd);
            m_Cursor = int(m_SequencePointsBuffer.size()) - 1;
#ifdef _DEBUG
            CleanCheck();
#endif

            if (m_notify)
            {
                m_notify->OnUndoStackChanged();
            }

            return cmd;
        }

        // by doing this, you take ownership of the memory!
        URSequencePoint* UndoStack::PopTop()
        {
            if (m_SequencePointsBuffer.empty())
            {
                return nullptr;
            }

            //CHB: Slice will notify if there is something to slice,
            //so we may not want to call it again below
            //however if it does not slice we just notify again below
            //so this may generate two calls so we may want to optimize this into one call
            //or something... remember if sliced notified then dont notify again maybe
            Slice();

            URSequencePoint* returned = m_SequencePointsBuffer[m_Cursor];
            m_SequencePointsBuffer.pop_back();
            returned->m_isPosted = false;
            m_Cursor = int(m_SequencePointsBuffer.size()) - 1;

            if (m_notify)
            {
                m_notify->OnUndoStackChanged();
            }

            return returned;
        }

        void UndoStack::SetClean()
        {
            m_CleanPoint = m_Cursor;

            if (m_notify)
            {
                m_notify->OnUndoStackChanged();
            }
#ifdef _DEBUG
            CleanCheck();
#endif
        }

        void UndoStack::Reset()
        {
            m_Cursor = m_CleanPoint = -1;
            for (AZStd::size_t idx = 0; idx < m_SequencePointsBuffer.size(); ++idx)
            {
                if (m_SequencePointsBuffer[idx])
                {
                    delete m_SequencePointsBuffer[idx];
                }
            }
            m_SequencePointsBuffer.clear();

            if (m_notify)
            {
                m_notify->OnUndoStackChanged();
            }

#ifdef _DEBUG
            CleanCheck();
#endif
        }

        URSequencePoint* UndoStack::Undo()
        {
            AZ_TracePrintf("Undo System", "Undo operation at cursor = %d and buffer size = %d\n", m_Cursor, int(m_SequencePointsBuffer.size()));

            AZ_Assert(!reentryGuard, "UndoStack operations are not reentrant");
            reentryGuard = true;

            if (m_Cursor >= 0)
            {
                m_SequencePointsBuffer[m_Cursor]->RunUndo();
                --m_Cursor;
                if (m_notify)
                {
                    m_notify->OnUndoStackChanged();
                }
            }
#ifdef _DEBUG
            CleanCheck();
#endif

            reentryGuard = false;
            return m_Cursor >= 0 ? m_SequencePointsBuffer[m_Cursor] : NULL;
        }
        URSequencePoint* UndoStack::Redo()
        {
            AZ_TracePrintf("Undo System", "Redo operation at cursor = %d and buffer size %d\n", m_Cursor, int(m_SequencePointsBuffer.size()));

            AZ_Assert(!reentryGuard, "UndoStack operations are not reentrant");
            reentryGuard = true;

            if (m_Cursor < int(m_SequencePointsBuffer.size()) - 1)
            {
                ++m_Cursor;
                m_SequencePointsBuffer[m_Cursor]->RunRedo();
#ifdef _DEBUG
                CleanCheck();
#endif
                if (m_notify)
                {
                    m_notify->OnUndoStackChanged();
                }
                reentryGuard = false;
                return m_SequencePointsBuffer[m_Cursor];
            }
#ifdef _DEBUG
            CleanCheck();
#endif

            reentryGuard = false;


            return NULL;
        }
        void UndoStack::Slice()
        {
            int difference = int(m_SequencePointsBuffer.size()) - 1 - m_Cursor;
            if (difference > 0)
            {
                for (int idx = m_Cursor + 1; idx < int(m_SequencePointsBuffer.size()); ++idx)
                {
                    delete m_SequencePointsBuffer[idx];
                    m_SequencePointsBuffer[idx] = NULL;
                }
                for (int idx = m_Cursor + 1; idx < int(m_SequencePointsBuffer.size()); )
                {
                    m_SequencePointsBuffer.pop_back();
                }

                if (m_CleanPoint > m_Cursor)
                {
                    // magic number deeper negative than the minimum -1 the cursor can reach
                    m_CleanPoint = -2;
#ifdef _DEBUG
                    CleanCheck();
#endif
                }
                if (m_notify)
                {
                    m_notify->OnUndoStackChanged();
                }
            }
        }

        URSequencePoint* UndoStack::Find(URCommandID ID, const AZ::Uuid& typeOfCommand)
        {
            for (int idx = 0; idx < int(m_SequencePointsBuffer.size()); ++idx)
            {
                URSequencePoint* cPtr = m_SequencePointsBuffer[idx]->Find(ID, typeOfCommand);
                if (cPtr)
                {
                    return cPtr;
                }
            }
            return NULL;
        }

        URSequencePoint* UndoStack::GetTop()
        {
            if (m_Cursor >= 0)
            {
                return m_SequencePointsBuffer[m_Cursor];
            }

            return nullptr;
        }

        bool UndoStack::IsClean() const
        {
            return m_Cursor == m_CleanPoint;
        }

        bool UndoStack::CanUndo() const
        {
            return (m_Cursor >= 0);
        }

        bool UndoStack::CanRedo() const
        {
            return (m_Cursor < int(m_SequencePointsBuffer.size()) - 1);
        }


        const char* UndoStack::GetRedoName() const
        {
            if (!CanRedo())
            {
                return NULL;
            }

            return m_SequencePointsBuffer[m_Cursor + 1]->GetName().c_str();
        }
        const char* UndoStack::GetUndoName() const
        {
            if (!CanUndo())
            {
                return NULL;
            }

            return m_SequencePointsBuffer[m_Cursor]->GetName().c_str();
        }

#ifdef _DEBUG
        void UndoStack::CleanCheck()
        {
            if (m_Cursor == m_CleanPoint)
            {
                // message CLEAN
                //AZ_TracePrintf("Undo System", "Clean undo cursor\n");
            }
            else
            {
                // message DIRTY
                //AZ_TracePrintf("Undo System", "Dirty undo cursor\n");
            }
        }
#endif
    }
} // namespace AzToolsFramework