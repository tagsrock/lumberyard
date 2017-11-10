
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
#ifndef ASSETBUILDERSDK_H
#define ASSETBUILDERSDK_H

#include <AzCore/std/string/string.h>
#include <AzCore/std/containers/vector.h>
#include <AZCore/std/parallel/atomic.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Serialization/SerializeContext.h>
#include "AssetBuilderBusses.h"

namespace AZ
{
    class ComponentDescriptor;
    class Entity;
}

namespace AssetBuilderSDK
{
    extern const char* const ErrorWindow; //Use this window name to log error messages.
    extern const char* const WarningWindow; //Use this window name to log warning messages.
    extern const char* const InfoWindow; //Use this window name to log info messages.

    // SubIDs uniquely identify a particular output product of a specific source asset
    // currently we use a scheme where various bits of the subId (which is a 32 bit unsigned) are used to designate different things.
    // we may expand this into a 64-bit "namespace" by adding additional 32 bits at the front at some point, if it becomes necessary.
    extern const AZ::u32 SUBID_MASK_ID;         //! mask is 0xFFFF - so you can have up to 64k subids from a single asset before you start running into the upper bits which are used for other reasons.
    extern const AZ::u32 SUBID_MASK_LOD_LEVEL;  //! the LOD level can be masked up to 15 LOD levels (it also represents the MIP level).  note that it starts at 1.
    extern const AZ::u32 SUBID_FLAG_DIFF;       //! this is a 'diff' map.  It may have the alpha, and lod set too if its an alpha of a diff
    extern const AZ::u32 SUBID_FLAG_ALPHA;      //! this is an alpha mip or alpha channel.

    //! extract only the ID using the above masks
    AZ::u32 GetSubID_ID(AZ::u32 packedSubId);
    //! extract only the LOD using the above masks.  note that it starts at 1, not 0.  0 would be the base asset.
    AZ::u32 GetSubID_LOD(AZ::u32 packedSubId);

    //! create a subid using the above masks.  Note that if you want to add additional bits such as DIFF or ALPHA, you must add them afterwards.
    //! fromsubindex contains an existing subindex to replace the LODs and SUBs but no other bits with.
    AZ::u32 ConstructSubID(AZ::u32 subIndex, AZ::u32 lodLevel, AZ::u32 fromSubIndex = 0);

    //! Initializes the serialization context with all the reflection information for AssetBuilderSDK structures
    //! Should be called on startup by standalone builders.  Builders run by AssetBuilder will have this set up already
    void InitializeSerializationContext();

    //! This method is used for logging builder related messages/error
    //! Do not use this inside ProcessJob, use AZ_TracePrintF instead.  This is only for general messages about your builder, not for job-specific messages
    extern void BuilderLog(AZ::Uuid builderId, const char* message, ...);

    //! Enum used by the builder for sending platform info
    enum Platform: AZ::u32
    {
        Platform_NONE       = 0x00,
        Platform_PC         = 0x01,
        Platform_ES3        = 0x02,
        Platform_IOS        = 0x04,
        Platform_OSX        = 0x08,
        Platform_XBOXONE    = 0x10,
        Platform_PS4        = 0x20,

        //! if you add a new platform entry to this enum, you must add it to allplatforms as well otherwise that platform would not be considered valid. 
        AllPlatforms = Platform_PC | Platform_ES3 | Platform_IOS | Platform_OSX | Platform_XBOXONE | Platform_PS4
    };

    //! Map data structure to holder parameters that are passed into a job for ProcessJob requests.
    //! These parameters can optionally be set during the create job function of the builder so that they are passed along
    //! to the ProcessJobFunction.  The values (key and value) are arbitrary and is up to the builder on how to use them
    typedef AZStd::unordered_map<AZ::u32, AZStd::string> JobParameterMap;

    //! Callback function type for creating jobs from job requests
    typedef AZStd::function<void(const CreateJobsRequest& request, CreateJobsResponse& response)> CreateJobFunction;

    //! Callback function type for processing jobs from process job requests
    typedef AZStd::function<void(const ProcessJobRequest& request, ProcessJobResponse& response)> ProcessJobFunction;

    //! Structure defining the type of pattern to use to apply
    struct AssetBuilderPattern
    {
        AZ_CLASS_ALLOCATOR(AssetBuilderPattern, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(AssetBuilderPattern, "{A8818121-D106-495E-9776-11F59E897BAD}");

        enum PatternType
        {
            //! The pattern is a file wildcard pattern (glob)
            Wildcard,
            //! The pattern is a regular expression pattern
            Regex
        };

        AZStd::string m_pattern;
        PatternType   m_type;

        AssetBuilderPattern() = default;
        AssetBuilderPattern(const AssetBuilderPattern& src) = default;
        AssetBuilderPattern(const AZStd::string& pattern, PatternType type);

        static void Reflect(AZ::ReflectContext* context);
    };

    //!Information that builders will send to the assetprocessor
    struct AssetBuilderDesc
    {
        //! The name of the Builder
        AZStd::string m_name;

        //! The collection of asset builder patterns that the builder will use to
        //! determine if a file will be processed by that builder
        AZStd::vector<AssetBuilderPattern>  m_patterns;

        //! The builder unique ID
        AZ::Uuid m_busId;

        //! Changing this version number will cause all your assets to be re-submitted to the builder for job creation and rebuilding.
        int m_version = 0;

        //! The required create job function callback that the asset processor will call during the job creation phase
        CreateJobFunction m_createJobFunction;
        //! The required process job function callback that the asset processor will call during the job processing phase
        ProcessJobFunction m_processJobFunction;
    };

    //! Source file dependency information that the builder will send to the assetprocessor
    //! It is important to note that the builder do not need to provide both the sourceFileDependencyUUID or sourceFileDependencyPath info to the asset processor,
    //! any one of them should be sufficient
    struct SourceFileDependency
    {
        AZ_CLASS_ALLOCATOR(SourceFileDependency, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(SourceFileDependency, "{d3c055d8-b5e8-44ab-a6ce-1ecb0da091ec}");

        //! Filepath on which the source file depends, it can be either be a relative or an absolute path. 
        //! if it's relative, the asset processor will check every watch folder in the order specified in the assetprocessor config file until it finds that file. 
        //! For example if the builder sends the sourcedependency info with sourceFileDependencyPath = "texture/blah.tiff" to the asset processor,
        //! it will check all watch folders for a file whose relative path with regard to it is "texture/blah.tiff".
        //! then "C:/dev/gamename/texture/blah.tiff" would be considered the source file dependency, if "C:/dev/gamename" is only watchfolder that contains such a file. 
        //! You can also send absolute path to the asset processor in which case the asset processor 
        //! will try to determine if there are any other file which overrides this file based on the watch folder order specified in the assetprocessor config file 
        //! and if an overriding file is found, then that file will be considered as the source dependency. 
        AZStd::string m_sourceFileDependencyPath;

        //!  UUID of the file on which the source file depends
        AZ::Uuid m_sourceFileDependencyUUID = AZ::Uuid::CreateNull();


        SourceFileDependency() {};

        SourceFileDependency(const AZStd::string& sourceFileDependencyPath, AZ::Uuid sourceFileDependencyUUID)
            : m_sourceFileDependencyPath(sourceFileDependencyPath)
            , m_sourceFileDependencyUUID(sourceFileDependencyUUID)
        {
        }

        SourceFileDependency(AZStd::string&& sourceFileDependencyPath, AZ::Uuid sourceFileDependencyUUID)
            : m_sourceFileDependencyPath(AZStd::move(sourceFileDependencyPath))
            , m_sourceFileDependencyUUID(sourceFileDependencyUUID)
        {
        }

        SourceFileDependency(const SourceFileDependency& other) = default;

        SourceFileDependency(SourceFileDependency&& other)
        {
            *this = AZStd::move(other);
        }

        SourceFileDependency& operator=(const SourceFileDependency& other);

        SourceFileDependency& operator=(SourceFileDependency&& other);

        static void Reflect(AZ::ReflectContext* context);
    };

    //! JobDescriptor is used by the builder to store job related information
    struct JobDescriptor
    {
        AZ_CLASS_ALLOCATOR(JobDescriptor, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(JobDescriptor, "{bd0472a4-7634-41f3-97ef-00f3b239bae2}");

        //! Any additional info that should be taken into account during fingerprinting for this job
        AZStd::string m_additionalFingerprintInfo;

        //! The target platform(s) that this job is configured for
        int m_platform = Platform_NONE;

        //! Job specific key, e.g. TIFF Job, etc
        AZStd::string m_jobKey;

        //! Flag to determine if this is a critical job or not.  Critical jobs are given the higher priority in the processing queue than non-critical jobs
        bool m_critical = false;

        //! Priority value for the jobs within the job queue.  If less than zero, than the priority of this job is not considered or or is lowest priority.
        //! If zero or greater, the value is prioritized by this number (the higher the number, the higher priority).  Note: priorities are set within critical
        //! and non-critical job separately.
        int m_priority = -1;

        //! Any builder specific parameters to pass to the Process Job Request
        JobParameterMap m_jobParameters;

        //! Flag to determine whether we need to check the input file for exclusive lock before we process the job
        bool m_checkExclusiveLock = false;
        JobDescriptor(AZStd::string additionalFingerprintInfo, int platform, AZStd::string jobKey);

        JobDescriptor();

        static void Reflect(AZ::ReflectContext* context);
    };

    //! RegisterBuilderRequest contains input data that will be sent by the AssetProcessor to the builder during the startup registration phase
    struct RegisterBuilderRequest
    {
        AZ_CLASS_ALLOCATOR(RegisterBuilderRequest, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(RegisterBuilderRequest, "{7C6C5198-4766-42B8-9A1E-48479CE2F5EA}");

        AZStd::string m_filePath;

        RegisterBuilderRequest() {}

        explicit RegisterBuilderRequest(const AZStd::string& filePath)
            : m_filePath(filePath)
        {
        }

        static void Reflect(AZ::ReflectContext* context);
    };

    //! RegisterBuilderResponse contains registration data that will be sent by the builder to the AssetProcessor in response to RegisterBuilderRequest
    struct RegisterBuilderResponse
    {
        AZ_CLASS_ALLOCATOR(RegisterBuilderResponse, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(RegisterBuilderResponse, "{0AE5583F-C763-410E-BA7F-78BD90546C01}");

        //! The name of the Builder
        AZStd::string m_name;

        //! The collection of asset builder patterns that the builder will use to
        //! determine if a file will be processed by that builder
        AZStd::vector<AssetBuilderPattern>  m_patterns;

        //! The builder unique ID
        AZ::Uuid m_busId;

        //! Changing this version number will cause all your assets to be re-submitted to the builder for job creation and rebuilding.
        int m_version = 0;

        RegisterBuilderResponse() {}

        static void Reflect(AZ::ReflectContext* context);
    };

    //! CreateJobsRequest contains input job data that will be send by the AssetProcessor to the builder for creating jobs
    struct CreateJobsRequest
    {
        AZ_CLASS_ALLOCATOR(CreateJobsRequest, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(CreateJobsRequest, "{02d470fb-4cb6-4cd7-876f-f0652910ff75}");

        //! The builder id to identify which builder will process this job request
        AZ::Uuid m_builderid; // builder id

        //! m_watchFolder contains the subfolder that the sourceFile came from, out of all the folders being watched by the Asset Processor.
        //! If you combine the Watch Folder with the Source File (m_sourceFile), you will result in the full absolute path to the file.
        AZStd::string m_watchFolder;

        //! The source file path that is relative to the watch folder (m_watchFolder)
        AZStd::string m_sourceFile;

        //! Platform flags informs the builder which platforms the AssetProcessor is interested in.
        int m_platformFlags;

        CreateJobsRequest();

        CreateJobsRequest(AZ::Uuid builderid, AZStd::string sourceFile, AZStd::string watchFolder, int platformFlags);

        // returns the number of platforms that are enabled for the source file
        size_t GetEnabledPlatformsCount() const;

        // returns the enabled platform by index, if no platform is found then we will return  Platform_NONE. 
        AssetBuilderSDK::Platform GetEnabledPlatformAt(size_t index) const;

        // determine whether the inputted platform is enabled or not, returns true if enabled  otherwise false  
        bool IsPlatformEnabled(AZ::u32 platform) const;

        // determine whether the inputted platform is valid or not, returns true if valid otherwise false
        bool IsPlatformValid(AZ::u32 platform) const;

        static void Reflect(AZ::ReflectContext* context);
    };

    //! Possible result codes from CreateJobs requests
    enum class CreateJobsResultCode
    {
        //! Jobs were created successfully
        Success,
        //! Jobs failed to be created
        Failed,
        //! The builder is in the process of shutting down
        ShuttingDown
    };

    //! CreateJobsResponse contains job data that will be send by the builder to the assetProcessor in response to CreateJobsRequest
    struct CreateJobsResponse
    {
        AZ_CLASS_ALLOCATOR(CreateJobsResponse, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(CreateJobsResponse, "{32a27d68-25bc-4425-a12b-bab961d6afcd}");

        CreateJobsResultCode         m_result = CreateJobsResultCode::Failed;   // The result code from the create jobs request

        AZStd::vector<SourceFileDependency> m_sourceFileDependencyList; // This is required for source files that want to declare dependencies on other source files.
        AZStd::vector<JobDescriptor> m_createJobOutputs;

        static void Reflect(AZ::ReflectContext* context);
    };


    //! JobProduct is used by the builder to store job product information
    struct JobProduct
    {
        AZ_CLASS_ALLOCATOR(JobProduct, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(JobProduct, "{d1d35d2c-3e4a-45c6-a13a-e20056344516}");

        AZStd::string m_productFileName; // relative or absolute product file path

        AZ::Data::AssetType m_productAssetType = AZ::Data::AssetType::CreateNull(); // the type of asset this is
        AZ::u32 m_productSubID; //a stable product identifier
        // SUB ID context: A Stable sub id means a few things. Products (game ready assets) are identified in the engine by AZ::Data::AssetId, which is a combination of source guid which is random and this product sub id. AssetType is currently NOT USED to differentiate assets by the system. So if two or more products of the same source are for the same platform they can not generate the same sub id!!! If they did this would be a COLLISION!!! which would not allow the rngine to access one or more of the products!!! Not using asset type in the differentiation may change in the future, but it is the way it is done for now.
        // SUB ID RULES:
        // 1. The builder alone is responsible for determining asset type and sub id.
        // 2. The sub id has to be build run stable, meaning if the builder were to run again for the same source the same sub id would be generated by the builder to identify this product.
        // 3. The sub id has to be location stable, meaning they can not be based on the location of the source or product, so if the source was moved to a different location it should still produce the same sub id for the same product.
        // 4. The sub id has to be platform stable, meaning if the builder were to make the equivalent product for a different platform the sub id for the equivalent product on the other platform should be the same.
        // 5. The sub id has to be multi output stable and mutually exclusive, meaning if your builder outputs multiple products from a source, the product sub id for each product must be different from one another and reproducible. So if you use an incrementing number scheme to differentiate products, that must also be stable, even when the source changes. So if a change occurs to the source, it gets rebuilt and the sub ids must still be the same. Put another way, if your builder outputs multiple product files, and produces the number and order and type of product, no matter what change to the source is made, then you're good. However, if changing the source may result in less or more products than last time, you may have a problem. The same products this time must have the same sub id as last time and can not have shifted up or down. Its ok if the extra product has the next new number, or if one less product is produced doesn't effect the others, in short they can never shift ids which would be the case for incrementing ids if one should no longer be produced. Note that the builder has no other information from run to run than the source data, it can not access any other data, source, product, database or otherwise receive data from any previous run. If the builder used an enumerated value for different outputs, that would work, say if he diffuse output always uses the enumerated value sub id 2 and the alpha always used 6, that should be fine, even if the source is modified such that it no longer outputs an alpha, the diffuse would still always map to 2.
        // SUGGESTIONS:
        // 1. If your builder only ever has one product for a source then we recommend that sub id be set to 0, this should satisfy all the above rules.
        // 2. Do not base sub id on file paths, if the location of source or destination changes the sub id will not be stable.
        // 3. Do not base sub id on source or product file name, extensions usually differ per platform and across platform they should be the stable.
        // 4. It might be ok to base sub id on extension-less product file name. It seems likely it would be stable as the product name would most likely be the same no matter its location as the path to the file and files extension could be different per platform and thus using only the extension-less file name would mostly likely be the same across platform. Be careful though, because if you output many same named files just with different extensions FOR THE SAME PLATFORM you will have collision problems.
        // 5. Basing the sub id on a simple incrementing number may be reasonable ONLY if order can never change, or the order if changed it would not matter. This may make sense for mip levels of textures if produced as separate products such that the sub id is equal to mip level, or lods for a mesh such that the sub id is the lod level.
        // 6. Think about using some other encoding scheme like using enumerations or using flag bits. If we do then we might be able to guess the sub id at runtime, that could be useful. Name spacing using the upper bits might be useful for final determination of product. This could be part of a localization scheme, or user settings options like choosing green blood via upper bits, or switching between products built by different builders which have stable lower bits and different name space upper bits. I am not currently convinced that encoding information into the sub id like this is a really great idea, however if it does not violate the rules, it is allowed, and it may solve a problem or two for specific systems.
        // 7. A Tagging system for products (even sources?) that allows the builder to add any tag it want to a product that would be available at tool time (and at runtime?) might be a better way than trying to encode that kind of data in product sub id's.

        JobProduct() {};
        JobProduct(const AZStd::string& productName, AZ::Data::AssetType productAssetType = AZ::Data::AssetType::CreateNull(), AZ::u32 productSubID = 0);
        JobProduct(AZStd::string&& productName, AZ::Data::AssetType productAssetType = AZ::Data::AssetType::CreateNull(), AZ::u32 productSubID = 0);
        JobProduct(const JobProduct& other) = default;

        JobProduct(JobProduct&& other);
        JobProduct& operator=(JobProduct&& other);

        JobProduct& operator=(const JobProduct& other) = default;
        //////////////////////////////////////////////////////////////////////////
        // Legacy compatiblity
        // when builders output asset type, but don't specify what type they actually are, we guess by file extension and other
        // markers.  This is not ideal.  If you're writing a new builder, endeavor to actually select a product asset type and a subId
        // that matches your needs.
        static AZ::Data::AssetType InferAssetTypeByProductFileName(const char* productFile);
        static AZ::u32 InferSubIDFromProductFileName(const AZ::Data::AssetType& assetType, const char* productFile);
        //////////////////////////////////////////////////////////////////////////

        static void Reflect(AZ::ReflectContext* context);
    };

    //! ProcessJobRequest contains input job data that will be send by the AssetProcessor to the builder for processing jobs
    struct ProcessJobRequest
    {
        AZ_CLASS_ALLOCATOR(ProcessJobRequest, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(ProcessJobRequest, "{20461454-d2f9-4079-ab95-703905e06002}");

        AZStd::string m_sourceFile; // relative source file name
        AZStd::string m_watchFolder; // watch folder for this source file
        AZStd::string m_fullPath; // full source file name
        AZ::Uuid m_builderGuid; // builder id
        JobDescriptor m_jobDescription; // job descriptor for this job.  Note that this still contains the job parameters from when you emitted it during CreateJobs
        AZStd::string m_tempDirPath; // temp directory that the builder should use to create job outputs for this job request
        AZ::u64 m_jobId; // job id for this job, this is also the address for the JobCancelListener
        AZStd::vector<SourceFileDependency> m_sourceFileDependencyList;

        ProcessJobRequest();

        static void Reflect(AZ::ReflectContext* context);
    };


    enum ProcessJobResultCode
    {
        ProcessJobResult_Success = 0,
        ProcessJobResult_Failed = 1,
        ProcessJobResult_Crashed = 2,
        ProcessJobResult_Cancelled = 3
    };

    //! ProcessJobResponse contains job data that will be send by the builder to the assetProcessor in response to ProcessJobRequest
    struct ProcessJobResponse
    {
        AZ_CLASS_ALLOCATOR(ProcessJobResponse, AZ::SystemAllocator, 0);
        AZ_TYPE_INFO(ProcessJobResponse, "{6b48ada5-0d52-43be-ad57-0bf8aeaef04b}");
        
        ProcessJobResultCode m_resultCode = ProcessJobResult_Success;
        AZStd::vector<JobProduct> m_outputProducts;

        ProcessJobResponse() = default;
        ProcessJobResponse(const ProcessJobResponse& other) = default;
        ProcessJobResponse(ProcessJobResponse&& other);

        ProcessJobResponse& operator=(const ProcessJobResponse& other);

        ProcessJobResponse& operator=(ProcessJobResponse&& other);

        static void Reflect(AZ::ReflectContext* context);
    };

    //! JobCancelListener can be used by builders in their processJob method to listen for job cancellation request.
    //! The address of this listener is the jobid which can be found in the process job request.
    class JobCancelListener : public JobCommandBus::Handler
    {
    public:
        explicit JobCancelListener(AZ::u64 jobId);
        ~JobCancelListener() override;
        JobCancelListener(const JobCancelListener&) = delete;
        //////////////////////////////////////////////////////////////////////////
        //!JobCommandBus::Handler overrides
        //!Note: This will be called on a thread other than your processing job thread. 
        //!You can derive from JobCancelListener and reimplement Cancel if you need to do something special in order to cancel your job.
        void Cancel() override;
        ///////////////////////////////////////////////////////////////////////

        bool IsCancelled() const;

    private:
        AZStd::atomic_bool m_cancelled;
    };
}

//! This macro should be used by every AssetBuilder to register itself,
//! AssetProcessor uses these exported function to identify whether a dll is an Asset Builder or not
//! If you want something highly custom you can do these entry points yourself instead of using the macro.
#define REGISTER_ASSETBUILDER                                                      \
    extern void BuilderOnInit();                                                   \
    extern void BuilderDestroy();                                                  \
    extern void BuilderRegisterDescriptors();                                      \
    extern void BuilderAddComponents(AZ::Entity * entity);                         \
    extern "C"                                                                     \
    {                                                                              \
    AZ_DLL_EXPORT int IsAssetBuilder()                                             \
    {                                                                              \
        return 0;                                                                  \
    }                                                                              \
                                                                                   \
    AZ_DLL_EXPORT void InitializeModule(AZ::EnvironmentInstance sharedEnvironment) \
    {                                                                              \
        AZ::Environment::Attach(sharedEnvironment);                                \
        BuilderOnInit();                                                           \
    }                                                                              \
                                                                                   \
    AZ_DLL_EXPORT void UninitializeModule()                                        \
    {                                                                              \
        BuilderDestroy();                                                          \
        AZ::Environment::Detach();                                                 \
    }                                                                              \
                                                                                   \
    AZ_DLL_EXPORT void ModuleRegisterDescriptors()                                 \
    {                                                                              \
        BuilderRegisterDescriptors();                                              \
    }                                                                              \
                                                                                   \
    AZ_DLL_EXPORT void ModuleAddComponents(AZ::Entity * entity)                    \
    {                                                                              \
        BuilderAddComponents(entity);                                              \
    }                                                                              \
    }

#endif //ASSETBUILDER_H