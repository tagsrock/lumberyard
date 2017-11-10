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

#include <AzCore/Asset/AssetTypeInfoBus.h>
#include <AzToolsFramework/AssetBrowser/AssetBrowserEntry.h>

#include <QObject>
#include <QString>
#include <QSharedPointer>

namespace AzToolsFramework
{
    namespace AssetBrowser
    {
        class AssetBrowserEntryFilter;
        typedef QSharedPointer<const AssetBrowserEntryFilter> FilterConstType;

        //////////////////////////////////////////////////////////////////////////
        // AssetBrowserEntryFilter
        //////////////////////////////////////////////////////////////////////////
        class AssetBrowserEntryFilter
            : public QObject
        {
            Q_OBJECT
        public:
            //! Propagate direction allows match satisfaction based on entry parents and/or children
            /*
                E.g. if PropagateDirection = Down, and entry does not satisfy filter, evaluation will propagate recursively to its children
            */
            enum PropagateDirection : int
            {
                None    = 0x00,
                Up      = 0x01,
                Down    = 0x02
            };

            AssetBrowserEntryFilter();
            virtual ~AssetBrowserEntryFilter() =  default;

            //! Check if entry matches filter
            bool Match(const AssetBrowserEntry* entry) const;

            //! Retrieve all matching entries that are either entry itself or its parents or children
            void Filter(AZStd::vector<const AssetBrowserEntry*>& result, const AssetBrowserEntry* entry) const;

            QString GetName() const;
            void SetName(const QString& name);

            //! Tags are used for identifying filters
            const QString& GetTag() const;
            void SetTag(const QString& tag);

            void SetFilterPropagation(int direction);

        Q_SIGNALS:
            void updatedSignal() const;

        protected:
            virtual QString GetNameInternal() const = 0;
            virtual bool MatchInternal(const AssetBrowserEntry* entry) const = 0;
            virtual void FilterInternal(AZStd::vector<const AssetBrowserEntry*>& result, const AssetBrowserEntry* entry) const;

        private:
            QString m_name;
            QString m_tag;
            int m_direction;

            bool MatchDown(const AssetBrowserEntry* entry) const;
            void FilterDown(AZStd::vector<const AssetBrowserEntry*>& result, const AssetBrowserEntry* entry) const;
        };


        //////////////////////////////////////////////////////////////////////////
        // StringFilter
        //////////////////////////////////////////////////////////////////////////
        //! StringFilter filters assets based on their name
        class StringFilter
            : public AssetBrowserEntryFilter
        {
            Q_OBJECT
        public:
            StringFilter();
            ~StringFilter() override = default;

            void SetFilterString(const QString& filterString);

        protected:
            QString GetNameInternal() const override;
            bool MatchInternal(const AssetBrowserEntry* entry) const override;

        private:
            QString m_filterString;
        };

        //////////////////////////////////////////////////////////////////////////
        // AssetTypeFilter
        //////////////////////////////////////////////////////////////////////////
        //! AssetTypeFilter filters products based on their asset id
        class AssetTypeFilter
            : public AssetBrowserEntryFilter
        {
            Q_OBJECT
        public:
            AssetTypeFilter();
            ~AssetTypeFilter() override = default;

            void SetAssetType(AZ::Data::AssetType assetType);
            void SetAssetType(const char* assetTypeName);
            AZ::Data::AssetType GetAssetType() const;

        protected:
            QString GetNameInternal() const override;
            bool MatchInternal(const AssetBrowserEntry* entry) const override;

        private:
            AZ::Data::AssetType m_assetType;
        };

        //////////////////////////////////////////////////////////////////////////
        // AssetGroupFilter
        //////////////////////////////////////////////////////////////////////////
        //! AssetGroupFilter filters products based on their asset group
        class AssetGroupFilter
            : public AssetBrowserEntryFilter
        {
            Q_OBJECT
        public:
            AssetGroupFilter();
            ~AssetGroupFilter() override = default;

            void SetAssetGroup(const QString& group);
            const QString& GetAssetTypeGroup() const;

        protected:
            QString GetNameInternal() const override;
            bool MatchInternal(const AssetBrowserEntry* entry) const override;

        private:
            QString m_group;
        };

        //////////////////////////////////////////////////////////////////////////
        // EntryTypeFilter
        //////////////////////////////////////////////////////////////////////////
        class EntryTypeFilter
            : public AssetBrowserEntryFilter
        {
            Q_OBJECT
        public:
            EntryTypeFilter();
            ~EntryTypeFilter() override = default;

            void SetEntryType(AssetBrowserEntry::AssetEntryType entryType);
            AssetBrowserEntry::AssetEntryType GetEntryType() const;

        protected:
            QString GetNameInternal() const override;
            bool MatchInternal(const AssetBrowserEntry* entry) const override;

        private:
            AssetBrowserEntry::AssetEntryType m_entryType;
        };

        //////////////////////////////////////////////////////////////////////////
        // CompositeFilter
        //////////////////////////////////////////////////////////////////////////
        //! CompositeFilter performs an AND/OR operation between multiple subfilters
        /*
            If more complex logic operations required, CompositeFilters can be nested
            with different logic operator types
        */
        class CompositeFilter
            : public AssetBrowserEntryFilter
        {
            Q_OBJECT
        public:
            enum class LogicOperatorType
            {
                OR,
                AND
            };

            explicit CompositeFilter(LogicOperatorType logicOperator);
            ~CompositeFilter() override = default;

            void AddFilter(FilterConstType filter);
            void RemoveFilter(FilterConstType filter);
            void RemoveAllFilters();
            void SetLogicOperator(LogicOperatorType logicOperator);
            const QList<FilterConstType>& GetSubFilters() const;
            //! Return value if there are no subfilters present
            void SetEmptyResult(bool result);

        protected:
            QString GetNameInternal() const override;
            bool MatchInternal(const AssetBrowserEntry* entry) const override;
            void FilterInternal(AZStd::vector<const AssetBrowserEntry*>& result, const AssetBrowserEntry* entry) const override;

        private:
            QList<FilterConstType> m_subFilters;
            LogicOperatorType m_logicOperator;
            bool m_emptyResult;
        };

        //////////////////////////////////////////////////////////////////////////
        // InverseFilter
        //////////////////////////////////////////////////////////////////////////
        //! Inverse filter negates result of its child filter
        class InverseFilter
            : public AssetBrowserEntryFilter
        {
            Q_OBJECT
        public:
            InverseFilter();
            ~InverseFilter() override = default;

            void SetFilter(FilterConstType filter);

        protected:
            QString GetNameInternal() const override;
            bool MatchInternal(const AssetBrowserEntry* entry) const override;
            void FilterInternal(AZStd::vector<const AssetBrowserEntry*>& result, const AssetBrowserEntry* entry) const override;

        private:
            FilterConstType m_filter;
        };
    } // namespace AssetBrowser
} // namespace AzToolsFramework
