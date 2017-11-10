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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#include "StdAfx.h"
#include "MaterialHelpers.h"

/* -----------------------------------------------------------------------
  * These functions are used in Cry3DEngine, CrySystem, CryRenderD3D11,
  * Editor, ResourceCompilerMaterial and more
  */

//////////////////////////////////////////////////////////////////////////
namespace
{
    static struct
    {
        EEfResTextures slot;
        const char* ename;
        bool adjustable;
        const char* name;
        const char* description;
        const char* suffix;
    }
    s_TexSlotSemantics[] =
    {
        // NOTE: must be in order with filled holes to allow direct lookup
        { EFTT_DIFFUSE,           "EFTT_DIFFUSE",           true,  "Diffuse"          , "Base surface color. Alpha mask is contained in alpha channel."          , "_diff" },
        { EFTT_NORMALS,           "EFTT_NORMALS",           true,  "Bumpmap"          , "Normal direction for each pixel simulating bumps on the surface. Smoothness map contained in alpha channel."          , "_ddn" }, // Ideally "Normal" but need to keep backwards-compatibility
        { EFTT_SPECULAR,          "EFTT_SPECULAR",          true,  "Specular"         , "Reflective and shininess intensity and color of reflective highlights"         , "_spec" },
        { EFTT_ENV,               "EFTT_ENV",               true,  "Environment"      , "Deprecated"    , "_cm" },
        { EFTT_DETAIL_OVERLAY,    "EFTT_DETAIL_OVERLAY",    true,  "Detail"           , "Increases micro and macro surface bump, diffuse and gloss detail. To use, enable the 'Detail Mapping' shader gen param. "           , "_detail" },
        { EFTT_SECOND_SMOOTHNESS, "EFTT_SECOND_SMOOTHNESS", false, "SecondSmoothness" , ""              , "" },
        { EFTT_HEIGHT,            "EFTT_HEIGHT",            true,  "Heightmap"        , "Height for offset bump, POM, silhouette POM, and displacement mapping defined by a Grayscale texture"        , "_displ" },
        { EFTT_DECAL_OVERLAY,     "EFTT_DECAL_OVERLAY",     true,  "Decal"            , ""              , "" }, // called "DecalOverlay" in the shaders
        { EFTT_SUBSURFACE,        "EFTT_SUBSURFACE",        true,  "SubSurface"       , ""              , "_sss" }, // called "Subsurface" in the shaders
        { EFTT_CUSTOM,            "EFTT_CUSTOM",            true,  "Custom"           , ""              , "" }, // called "CustomMap" in the shaders
        { EFTT_CUSTOM_SECONDARY,  "EFTT_CUSTOM_SECONDARY",  true,  "[1] Custom"       , ""              , "" },
        { EFTT_OPACITY,           "EFTT_OPACITY",           true,  "Opacity"          , "SubSurfaceScattering map to simulate thin areas for light to penetrate"          , "" },
        { EFTT_SMOOTHNESS,        "EFTT_SMOOTHNESS",        false, "Smoothness"       , ""              , "_ddna" },
        { EFTT_EMITTANCE,         "EFTT_EMITTANCE",         true,  "Emittance"        , "Multiplies the emissive color with RGB texture. Emissive alpha mask is contained in alpha channel."        , "_em" },
        { EFTT_OCCLUSION,         "EFTT_OCCLUSION",         true,  "Occlusion"        , "Grayscale texture to mask diffuse lighting response and simulate darker areas"        , "" },
        { EFTT_SPECULAR_2,        "EFTT_SPECULAR_2",        true,  "Specular2"        , ""              , "_spec"   },

        // Backwards compatible names are found here and mapped to the updated enum
        { EFTT_NORMALS,           "EFTT_BUMP",              false, "Normal"           , ""              , "" }, // called "Bump" in the shaders
        { EFTT_SMOOTHNESS,        "EFTT_GLOSS_NORMAL_A",    false, "GlossNormalA"     , ""              , "" },
        { EFTT_HEIGHT,            "EFTT_BUMPHEIGHT",        false, "Height"           , ""              , ""        }, // called "BumpHeight" in the shaders

        // This is the terminator for the name-search
        { EFTT_UNKNOWN,          "EFTT_UNKNOWN",          false, NULL          , ""        },
    };

#if 0
    static class Verify
    {
    public:
        Verify()
        {
            for (int i = 0; s_TexSlotSemantics[i].name; i++)
            {
                if (s_TexSlotSemantics[i].slot != i)
                {
                    throw std::runtime_error("Invalid texture slot lookup array.");
                }
            }
        }
    }
    s_VerifyTexSlotSemantics;
#endif
}

EEfResTextures MaterialHelpers::FindTexSlot(const char* texName) const
{
    for (int i = 0; s_TexSlotSemantics[i].name; i++)
    {
        if (stricmp(s_TexSlotSemantics[i].name, texName) == 0)
        {
            return s_TexSlotSemantics[i].slot;
        }
    }

    return EFTT_UNKNOWN;
}

const char* MaterialHelpers::FindTexName(EEfResTextures texSlot) const
{
    for (int i = 0; s_TexSlotSemantics[i].name; i++)
    {
        if (s_TexSlotSemantics[i].slot == texSlot)
        {
            return s_TexSlotSemantics[i].name;
        }
    }

    return NULL;
}

const char* MaterialHelpers::LookupTexName(EEfResTextures texSlot) const
{
    assert((texSlot >= 0) && (texSlot < EFTT_MAX));
    return s_TexSlotSemantics[texSlot].name;
}

const char* MaterialHelpers::LookupTexDesc(EEfResTextures texSlot) const
{
    assert((texSlot >= 0) && (texSlot < EFTT_MAX));
    return s_TexSlotSemantics[texSlot].description;
}

const char* MaterialHelpers::LookupTexEnum(EEfResTextures texSlot) const
{
    assert((texSlot >= 0) && (texSlot < EFTT_MAX));
    return s_TexSlotSemantics[texSlot].ename;
}

const char* MaterialHelpers::LookupTexSuffix(EEfResTextures texSlot) const
{
    assert((texSlot >= 0) && (texSlot < EFTT_MAX));
    return s_TexSlotSemantics[texSlot].suffix;
}

bool MaterialHelpers::IsAdjustableTexSlot(EEfResTextures texSlot) const
{
    assert((texSlot >= 0) && (texSlot < EFTT_MAX));
    return s_TexSlotSemantics[texSlot].adjustable;
}

//////////////////////////////////////////////////////////////////////////
bool MaterialHelpers::SetGetMaterialParamFloat(IRenderShaderResources& pShaderResources, const char* sParamName, float& v, bool bGet) const
{
    EEfResTextures texSlot = EFTT_UNKNOWN;

    if (!stricmp("emissive_intensity", sParamName))
    {
        texSlot = EFTT_EMITTANCE;
    }
    else if (!stricmp("shininess", sParamName))
    {
        texSlot = EFTT_SMOOTHNESS;
    }
    else if (!stricmp("opacity", sParamName))
    {
        texSlot = EFTT_OPACITY;
    }

    if (!stricmp("alpha", sParamName))
    {
        if (bGet)
        {
            v = pShaderResources.GetAlphaRef();
        }
        else
        {
            pShaderResources.SetAlphaRef(v);
        }

        return true;
    }
    else if (texSlot != EFTT_UNKNOWN)
    {
        if (bGet)
        {
            v = pShaderResources.GetStrengthValue(texSlot);
        }
        else
        {
            pShaderResources.SetStrengthValue(texSlot, v);
        }

        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
bool MaterialHelpers::SetGetMaterialParamVec3(IRenderShaderResources& pShaderResources, const char* sParamName, Vec3& v, bool bGet) const
{
    EEfResTextures texSlot = EFTT_UNKNOWN;

    if (!stricmp("diffuse", sParamName))
    {
        texSlot = EFTT_DIFFUSE;
    }
    else if (!stricmp("specular", sParamName))
    {
        texSlot = EFTT_SPECULAR;
    }
    else if (!stricmp("emissive_color", sParamName))
    {
        texSlot = EFTT_EMITTANCE;
    }

    if (texSlot != EFTT_UNKNOWN)
    {
        if (bGet)
        {
            v = pShaderResources.GetColorValue(texSlot).toVec3();
        }
        else
        {
            pShaderResources.SetColorValue(texSlot, ColorF(v, 1.0f));
        }

        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
void MaterialHelpers::SetTexModFromXml(SEfTexModificator& pTextureModifier, const XmlNodeRef& node) const
{
    XmlNodeRef modNode = node->findChild("TexMod");
    if (modNode)
    {
        // Modificators
        float f;
        uint8 c;

        modNode->getAttr("TexMod_RotateType", pTextureModifier.m_eRotType);
        modNode->getAttr("TexMod_TexGenType", pTextureModifier.m_eTGType);
        modNode->getAttr("TexMod_bTexGenProjected", pTextureModifier.m_bTexGenProjected);

        for (int baseu = 'U', u = baseu; u <= 'W'; u++)
        {
            char RT[] = "Rotate?";
            RT[6] = u;

            if (modNode->getAttr(RT, f))
            {
                pTextureModifier.m_Rot            [u - baseu] = Degr2Word(f);
            }

            char RR[] = "TexMod_?RotateRate";
            RR[7] = u;
            char RP[] = "TexMod_?RotatePhase";
            RP[7] = u;
            char RA[] = "TexMod_?RotateAmplitude";
            RA[7] = u;
            char RC[] = "TexMod_?RotateCenter";
            RC[7] = u;

            if (modNode->getAttr(RR, f))
            {
                pTextureModifier.m_RotOscRate     [u - baseu] = Degr2Word(f);
            }
            if (modNode->getAttr(RP, f))
            {
                pTextureModifier.m_RotOscPhase    [u - baseu] = Degr2Word(f);
            }
            if (modNode->getAttr(RA, f))
            {
                pTextureModifier.m_RotOscAmplitude[u - baseu] = Degr2Word(f);
            }
            if (modNode->getAttr(RC, f))
            {
                pTextureModifier.m_RotOscCenter   [u - baseu] =           f;
            }

            if (u > 'V')
            {
                continue;
            }

            char TL[] = "Tile?";
            TL[4] = u;
            char OF[] = "Offset?";
            OF[6] = u;

            if (modNode->getAttr(TL, f))
            {
                pTextureModifier.m_Tiling         [u - baseu] = f;
            }
            if (modNode->getAttr(OF, f))
            {
                pTextureModifier.m_Offs           [u - baseu] = f;
            }

            char OT[] = "TexMod_?OscillatorType";
            OT[7] = u;
            char OR[] = "TexMod_?OscillatorRate";
            OR[7] = u;
            char OP[] = "TexMod_?OscillatorPhase";
            OP[7] = u;
            char OA[] = "TexMod_?OscillatorAmplitude";
            OA[7] = u;

            if (modNode->getAttr(OT, c))
            {
                pTextureModifier.m_eMoveType      [u - baseu] = c;
            }
            if (modNode->getAttr(OR, f))
            {
                pTextureModifier.m_OscRate        [u - baseu] = f;
            }
            if (modNode->getAttr(OP, f))
            {
                pTextureModifier.m_OscPhase       [u - baseu] = f;
            }
            if (modNode->getAttr(OA, f))
            {
                pTextureModifier.m_OscAmplitude   [u - baseu] = f;
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
static SEfTexModificator defaultTexMod;
static bool defaultTexMod_Initialized = false;

void MaterialHelpers::SetXmlFromTexMod(const SEfTexModificator& pTextureModifier, XmlNodeRef& node) const
{
    if (!defaultTexMod_Initialized)
    {
        ZeroStruct(defaultTexMod);
        defaultTexMod.m_Tiling[0] = 1;
        defaultTexMod.m_Tiling[1] = 1;
        defaultTexMod_Initialized = true;
    }

    if (memcmp(&pTextureModifier, &defaultTexMod, sizeof(pTextureModifier)) == 0)
    {
        return;
    }

    XmlNodeRef modNode = node->newChild("TexMod");
    if (modNode)
    {
        // Modificators
        float f;
        uint16 s;
        uint8 c;

        modNode->setAttr("TexMod_RotateType", pTextureModifier.m_eRotType);
        modNode->setAttr("TexMod_TexGenType", pTextureModifier.m_eTGType);
        modNode->setAttr("TexMod_bTexGenProjected", pTextureModifier.m_bTexGenProjected);

        for (int baseu = 'U', u = baseu; u <= 'W'; u++)
        {
            char RT[] = "Rotate?";
            RT[6] = u;

            if ((s = pTextureModifier.m_Rot            [u - baseu]) != defaultTexMod.m_Rot            [u - baseu])
            {
                modNode->setAttr(RT, Word2Degr(s));
            }

            char RR[] = "TexMod_?RotateRate";
            RR[7] = u;
            char RP[] = "TexMod_?RotatePhase";
            RP[7] = u;
            char RA[] = "TexMod_?RotateAmplitude";
            RA[7] = u;
            char RC[] = "TexMod_?RotateCenter";
            RC[7] = u;

            if ((s = pTextureModifier.m_RotOscRate     [u - baseu]) != defaultTexMod.m_RotOscRate     [u - baseu])
            {
                modNode->setAttr(RR, Word2Degr(s));
            }
            if ((s = pTextureModifier.m_RotOscPhase    [u - baseu]) != defaultTexMod.m_RotOscPhase    [u - baseu])
            {
                modNode->setAttr(RP, Word2Degr(s));
            }
            if ((s = pTextureModifier.m_RotOscAmplitude[u - baseu]) != defaultTexMod.m_RotOscAmplitude[u - baseu])
            {
                modNode->setAttr(RA, Word2Degr(s));
            }
            if ((f = pTextureModifier.m_RotOscCenter   [u - baseu]) != defaultTexMod.m_RotOscCenter   [u - baseu])
            {
                modNode->setAttr(RC,           f);
            }

            if (u > 'V')
            {
                continue;
            }

            char TL[] = "Tile?";
            TL[4] = u;
            char OF[] = "Offset?";
            OF[6] = u;

            if ((f = pTextureModifier.m_Tiling         [u - baseu]) != defaultTexMod.m_Tiling         [u - baseu])
            {
                modNode->setAttr(TL, f);
            }
            if ((f = pTextureModifier.m_Offs           [u - baseu]) != defaultTexMod.m_Offs           [u - baseu])
            {
                modNode->setAttr(OF, f);
            }

            char OT[] = "TexMod_?OscillatorType";
            OT[7] = u;
            char OR[] = "TexMod_?OscillatorRate";
            OR[7] = u;
            char OP[] = "TexMod_?OscillatorPhase";
            OP[7] = u;
            char OA[] = "TexMod_?OscillatorAmplitude";
            OA[7] = u;

            if ((c = pTextureModifier.m_eMoveType      [u - baseu]) != defaultTexMod.m_eMoveType      [u - baseu])
            {
                modNode->setAttr(OT, c);
            }
            if ((f = pTextureModifier.m_OscRate        [u - baseu]) != defaultTexMod.m_OscRate        [u - baseu])
            {
                modNode->setAttr(OR, f);
            }
            if ((f = pTextureModifier.m_OscPhase       [u - baseu]) != defaultTexMod.m_OscPhase       [u - baseu])
            {
                modNode->setAttr(OP, f);
            }
            if ((f = pTextureModifier.m_OscAmplitude   [u - baseu]) != defaultTexMod.m_OscAmplitude   [u - baseu])
            {
                modNode->setAttr(OA, f);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void MaterialHelpers::SetTexturesFromXml(SInputShaderResources& pShaderResources, const XmlNodeRef& node) const
{
    const char* texmap = "";
    const char* file = "";

    XmlNodeRef texturesNode = node->findChild("Textures");
    if (texturesNode)
    {
        for (int c = 0; c < texturesNode->getChildCount(); c++)
        {
            XmlNodeRef texNode = texturesNode->getChild(c);
            texmap = texNode->getAttr("Map");

            EEfResTextures texId = MaterialHelpers::FindTexSlot(texmap);
            if (texId == EFTT_UNKNOWN)
            {
                continue;
            }

            file = texNode->getAttr("File");

            // legacy.  Some textures used to be referenced using "engine\\" or "engine/" - this is no longer valid
            if (
                (strlen(file) > 7) &&
                (strnicmp(file, "engine", 6) == 0) &&
                ((file[6] == '\\') || (file[6] == '/'))
                )
            {
                file = file + 7;
            }

            // legacy:  Files were saved into a mtl with many leading forward or back slashes, we eat them all here.  We want it to start with a rel path.
            const char* actualFileName = file;
            while ((actualFileName[0]) && ((actualFileName[0] == '\\') || (actualFileName[0] == '/')))
            {
                ++actualFileName;
            }
            file = actualFileName;


            // Correct texid found.
            pShaderResources.m_Textures[texId].m_Name = file;

            texNode->getAttr("IsTileU", pShaderResources.m_Textures[texId].m_bUTile);
            texNode->getAttr("IsTileV", pShaderResources.m_Textures[texId].m_bVTile);
            texNode->getAttr("TexType", pShaderResources.m_Textures[texId].m_Sampler.m_eTexType);

            int filter = pShaderResources.m_Textures[texId].m_Filter;
            if (texNode->getAttr("Filter", filter))
            {
                pShaderResources.m_Textures[texId].m_Filter = filter;
            }

            SetTexModFromXml(*pShaderResources.m_Textures[texId].AddModificator(), texNode);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
static SInputShaderResources defaultShaderResource;

void MaterialHelpers::SetXmlFromTextures(const SInputShaderResources& pShaderResources, XmlNodeRef& node) const
{
    // Save texturing data.
    XmlNodeRef texturesNode = node->newChild("Textures");
    for (EEfResTextures texId = EEfResTextures(0); texId < EFTT_MAX; texId = EEfResTextures(texId + 1))
    {
        if (!pShaderResources.m_Textures[texId].m_Name.empty())
        {
            XmlNodeRef texNode = texturesNode->newChild("Texture");

            //    texNode->setAttr("TexID",texId);
            texNode->setAttr("Map", MaterialHelpers::LookupTexName(texId));
            texNode->setAttr("File", pShaderResources.m_Textures[texId].m_Name.c_str());

            if (pShaderResources.m_Textures[texId].m_Filter != defaultShaderResource.m_Textures[texId].m_Filter)
            {
                texNode->setAttr("Filter", pShaderResources.m_Textures[texId].m_Filter);
            }
            if (pShaderResources.m_Textures[texId].m_bUTile != defaultShaderResource.m_Textures[texId].m_bUTile)
            {
                texNode->setAttr("IsTileU", pShaderResources.m_Textures[texId].m_bUTile);
            }
            if (pShaderResources.m_Textures[texId].m_bVTile != defaultShaderResource.m_Textures[texId].m_bVTile)
            {
                texNode->setAttr("IsTileV", pShaderResources.m_Textures[texId].m_bVTile);
            }
            if (pShaderResources.m_Textures[texId].m_Sampler.m_eTexType != defaultShaderResource.m_Textures[texId].m_Sampler.m_eTexType)
            {
                texNode->setAttr("TexType", pShaderResources.m_Textures[texId].m_Sampler.m_eTexType);
            }

            //////////////////////////////////////////////////////////////////////////
            // Save texture modificators Modificators
            //////////////////////////////////////////////////////////////////////////
            SetXmlFromTexMod(*pShaderResources.m_Textures[texId].GetModificator(), texNode);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void MaterialHelpers::SetVertexDeformFromXml(SInputShaderResources& pShaderResources, const XmlNodeRef& node) const
{
    if (defaultShaderResource.m_DeformInfo.m_eType != pShaderResources.m_DeformInfo.m_eType)
    {
        node->setAttr("vertModifType", pShaderResources.m_DeformInfo.m_eType);
    }

    XmlNodeRef deformNode = node->findChild("VertexDeform");
    if (deformNode)
    {
        int deform_type = eDT_Unknown;
        deformNode->getAttr("Type", deform_type);
        pShaderResources.m_DeformInfo.m_eType = (EDeformType)deform_type;
        deformNode->getAttr("DividerX", pShaderResources.m_DeformInfo.m_fDividerX);
        deformNode->getAttr("DividerY", pShaderResources.m_DeformInfo.m_fDividerY);
        deformNode->getAttr("DividerZ", pShaderResources.m_DeformInfo.m_fDividerZ);
        deformNode->getAttr("DividerW", pShaderResources.m_DeformInfo.m_fDividerW);
        deformNode->getAttr("NoiseScale", pShaderResources.m_DeformInfo.m_vNoiseScale);

        XmlNodeRef waveX = deformNode->findChild("WaveX");
        if (waveX)
        {
            int type = eWF_None;
            waveX->getAttr("Type", type);
            pShaderResources.m_DeformInfo.m_WaveX.m_eWFType = (EWaveForm)type;
            waveX->getAttr("Amp", pShaderResources.m_DeformInfo.m_WaveX.m_Amp);
            waveX->getAttr("Level", pShaderResources.m_DeformInfo.m_WaveX.m_Level);
            waveX->getAttr("Phase", pShaderResources.m_DeformInfo.m_WaveX.m_Phase);
            waveX->getAttr("Freq", pShaderResources.m_DeformInfo.m_WaveX.m_Freq);
        }

        XmlNodeRef waveY = deformNode->findChild("WaveY");
        if (waveY)
        {
            int type = eWF_None;
            waveY->getAttr("Type", type);
            pShaderResources.m_DeformInfo.m_WaveY.m_eWFType = (EWaveForm)type;
            waveY->getAttr("Amp", pShaderResources.m_DeformInfo.m_WaveY.m_Amp);
            waveY->getAttr("Level", pShaderResources.m_DeformInfo.m_WaveY.m_Level);
            waveY->getAttr("Phase", pShaderResources.m_DeformInfo.m_WaveY.m_Phase);
            waveY->getAttr("Freq", pShaderResources.m_DeformInfo.m_WaveY.m_Freq);
        }

        XmlNodeRef waveZ = deformNode->findChild("WaveZ");
        if (waveZ)
        {
            int type = eWF_None;
            waveZ->getAttr("Type", type);
            pShaderResources.m_DeformInfo.m_WaveZ.m_eWFType = (EWaveForm)type;
            waveZ->getAttr("Amp", pShaderResources.m_DeformInfo.m_WaveZ.m_Amp);
            waveZ->getAttr("Level", pShaderResources.m_DeformInfo.m_WaveZ.m_Level);
            waveZ->getAttr("Phase", pShaderResources.m_DeformInfo.m_WaveZ.m_Phase);
            waveZ->getAttr("Freq", pShaderResources.m_DeformInfo.m_WaveZ.m_Freq);
        }

        XmlNodeRef waveW = deformNode->findChild("WaveW");
        if (waveW)
        {
            int type = eWF_None;
            waveW->getAttr("Type", type);
            pShaderResources.m_DeformInfo.m_WaveW.m_eWFType = (EWaveForm)type;
            waveW->getAttr("Amp", pShaderResources.m_DeformInfo.m_WaveW.m_Amp);
            waveW->getAttr("Level", pShaderResources.m_DeformInfo.m_WaveW.m_Level);
            waveW->getAttr("Phase", pShaderResources.m_DeformInfo.m_WaveW.m_Phase);
            waveW->getAttr("Freq", pShaderResources.m_DeformInfo.m_WaveW.m_Freq);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void MaterialHelpers::SetXmlFromVertexDeform(const SInputShaderResources& pShaderResources, XmlNodeRef& node) const
{
    int vertModif = pShaderResources.m_DeformInfo.m_eType;
    node->setAttr("vertModifType", vertModif);

    if (pShaderResources.m_DeformInfo.m_eType != eDT_Unknown)
    {
        XmlNodeRef deformNode = node->newChild("VertexDeform");

        deformNode->setAttr("Type", pShaderResources.m_DeformInfo.m_eType);
        deformNode->setAttr("DividerX", pShaderResources.m_DeformInfo.m_fDividerX);
        deformNode->setAttr("DividerY", pShaderResources.m_DeformInfo.m_fDividerY);
        deformNode->setAttr("NoiseScale", pShaderResources.m_DeformInfo.m_vNoiseScale);

        if (pShaderResources.m_DeformInfo.m_WaveX.m_eWFType != eWF_None)
        {
            XmlNodeRef waveX = deformNode->newChild("WaveX");
            waveX->setAttr("Type", pShaderResources.m_DeformInfo.m_WaveX.m_eWFType);
            waveX->setAttr("Amp", pShaderResources.m_DeformInfo.m_WaveX.m_Amp);
            waveX->setAttr("Level", pShaderResources.m_DeformInfo.m_WaveX.m_Level);
            waveX->setAttr("Phase", pShaderResources.m_DeformInfo.m_WaveX.m_Phase);
            waveX->setAttr("Freq", pShaderResources.m_DeformInfo.m_WaveX.m_Freq);
        }

        if (pShaderResources.m_DeformInfo.m_WaveY.m_eWFType != eWF_None)
        {
            XmlNodeRef waveY = deformNode->newChild("WaveY");
            waveY->setAttr("Type", pShaderResources.m_DeformInfo.m_WaveY.m_eWFType);
            waveY->setAttr("Amp", pShaderResources.m_DeformInfo.m_WaveY.m_Amp);
            waveY->setAttr("Level", pShaderResources.m_DeformInfo.m_WaveY.m_Level);
            waveY->setAttr("Phase", pShaderResources.m_DeformInfo.m_WaveY.m_Phase);
            waveY->setAttr("Freq", pShaderResources.m_DeformInfo.m_WaveY.m_Freq);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
static inline ColorF ToCFColor(const Vec3& col)
{
    return ColorF(col);
}

void MaterialHelpers::SetLightingFromXml(SInputShaderResources& pShaderResources, const XmlNodeRef& node) const
{
    // Load lighting data.
    Vec3 vColor;
    Vec4 vColor4;
    if (node->getAttr("Diffuse", vColor4))
    {
        pShaderResources.m_LMaterial.m_Diffuse = ColorF(vColor4.x, vColor4.y, vColor4.z, vColor4.w);
    }
    else if (node->getAttr("Diffuse", vColor))
    {
        pShaderResources.m_LMaterial.m_Diffuse = ToCFColor(vColor);
    }
    if (node->getAttr("Specular", vColor4))
    {
        pShaderResources.m_LMaterial.m_Specular = ColorF(vColor4.x, vColor4.y, vColor4.z, vColor4.w);
    }
    else if (node->getAttr("Specular", vColor))
    {
        pShaderResources.m_LMaterial.m_Specular = ToCFColor(vColor);
    }
    if (node->getAttr("Emittance", vColor4))
    {
        pShaderResources.m_LMaterial.m_Emittance = ColorF(vColor4.x, vColor4.y, vColor4.z, vColor4.w);
    }

    node->getAttr("Shininess", pShaderResources.m_LMaterial.m_Smoothness);
    node->getAttr("Opacity", pShaderResources.m_LMaterial.m_Opacity);
    node->getAttr("AlphaTest", pShaderResources.m_AlphaRef);
    node->getAttr("VoxelCoverage", pShaderResources.m_VoxelCoverage);
}

//////////////////////////////////////////////////////////////////////////
static inline Vec3 ToVec3(const ColorF& col)
{
    return Vec3(col.r, col.g, col.b);
}

static inline Vec4 ToVec4(const ColorF& col)
{
    return Vec4(col.r, col.g, col.b, col.a);
}

void MaterialHelpers::SetXmlFromLighting(const SInputShaderResources& pShaderResources, XmlNodeRef& node) const
{
    // Save ligthing data.
    if (defaultShaderResource.m_LMaterial.m_Diffuse != pShaderResources.m_LMaterial.m_Diffuse)
    {
        node->setAttr("Diffuse", ToVec4(pShaderResources.m_LMaterial.m_Diffuse));
    }
    if (defaultShaderResource.m_LMaterial.m_Specular != pShaderResources.m_LMaterial.m_Specular)
    {
        node->setAttr("Specular", ToVec4(pShaderResources.m_LMaterial.m_Specular));
    }
    if (defaultShaderResource.m_LMaterial.m_Emittance != pShaderResources.m_LMaterial.m_Emittance)
    {
        node->setAttr("Emittance", ToVec4(pShaderResources.m_LMaterial.m_Emittance));
    }

    if (defaultShaderResource.m_LMaterial.m_Opacity != pShaderResources.m_LMaterial.m_Opacity)
    {
        node->setAttr("Opacity", pShaderResources.m_LMaterial.m_Opacity);
    }
    if (defaultShaderResource.m_LMaterial.m_Smoothness != pShaderResources.m_LMaterial.m_Smoothness)
    {
        node->setAttr("Shininess", pShaderResources.m_LMaterial.m_Smoothness);
    }

    if (defaultShaderResource.m_AlphaRef != pShaderResources.m_AlphaRef)
    {
        node->setAttr("AlphaTest", pShaderResources.m_AlphaRef);
    }
    if (defaultShaderResource.m_VoxelCoverage != pShaderResources.m_VoxelCoverage)
    {
        node->setAttr("VoxelCoverage", pShaderResources.m_VoxelCoverage);
    }
}

//////////////////////////////////////////////////////////////////////////
void MaterialHelpers::SetShaderParamsFromXml(SInputShaderResources& pShaderResources, const XmlNodeRef& node) const
{
    int nA = node->getNumAttributes();
    if (!nA)
    {
        return;
    }

    for (int i = 0; i < nA; i++)
    {
        const char* key = NULL, * val = NULL;
        node->getAttributeByIndex(i, &key, &val);

        // try to set existing param first
        bool bFound = false;

        for (int i = 0; i < pShaderResources.m_ShaderParams.size(); i++)
        {
            SShaderParam* pParam = &pShaderResources.m_ShaderParams[i];

            if (strcmp(pParam->m_Name, key) == 0)
            {
                bFound = true;

                switch (pParam->m_Type)
                {
                case eType_BYTE:
                    node->getAttr(key, pParam->m_Value.m_Byte);
                    break;
                case eType_SHORT:
                    node->getAttr(key, pParam->m_Value.m_Short);
                    break;
                case eType_INT:
                    node->getAttr(key, pParam->m_Value.m_Int);
                    break;
                case eType_FLOAT:
                    node->getAttr(key, pParam->m_Value.m_Float);
                    break;
                case eType_FCOLOR:
                case eType_FCOLORA:
                {
                    Vec3 vValue;
                    node->getAttr(key, vValue);

                    pParam->m_Value.m_Color[0] = vValue.x;
                    pParam->m_Value.m_Color[1] = vValue.y;
                    pParam->m_Value.m_Color[2] = vValue.z;
                }
                break;
                case eType_VECTOR:
                {
                    Vec4 vValue;
                    if (node->getAttr(key, vValue))
                    {
                        pParam->m_Value.m_Color[0] = vValue.x;
                        pParam->m_Value.m_Color[1] = vValue.y;
                        pParam->m_Value.m_Color[2] = vValue.z;
                        pParam->m_Value.m_Color[3] = vValue.w;
                    }
                    else
                    {
                        Vec3 vValue3;
                        if (node->getAttr(key, vValue3))
                        {
                            pParam->m_Value.m_Color[0] = vValue3.x;
                            pParam->m_Value.m_Color[1] = vValue3.y;
                            pParam->m_Value.m_Color[2] = vValue3.z;
                            pParam->m_Value.m_Color[3] = 1.0f;
                        }
                    }
                }
                break;
                default:
                    break;
                }
            }
        }

        if (!bFound)
        {
            assert(val && key);

            SShaderParam Param;
            cry_strcpy(Param.m_Name, key);
            Param.m_Value.m_Color[0] = Param.m_Value.m_Color[1] = Param.m_Value.m_Color[2] = Param.m_Value.m_Color[3] = 0;

            int res = sscanf(val, "%f,%f,%f,%f", &Param.m_Value.m_Color[0], &Param.m_Value.m_Color[1], &Param.m_Value.m_Color[2], &Param.m_Value.m_Color[3]);
            assert(res);

            pShaderResources.m_ShaderParams.push_back(Param);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void MaterialHelpers::SetXmlFromShaderParams(const SInputShaderResources& pShaderResources, XmlNodeRef& node) const
{
    for (int i = 0; i < pShaderResources.m_ShaderParams.size(); i++)
    {
        const SShaderParam* pParam = &pShaderResources.m_ShaderParams[i];
        switch (pParam->m_Type)
        {
        case eType_BYTE:
            node->setAttr(pParam->m_Name, (int)pParam->m_Value.m_Byte);
            break;
        case eType_SHORT:
            node->setAttr(pParam->m_Name, (int)pParam->m_Value.m_Short);
            break;
        case eType_INT:
            node->setAttr(pParam->m_Name, (int)pParam->m_Value.m_Int);
            break;
        case eType_FLOAT:
            node->setAttr(pParam->m_Name, (float)pParam->m_Value.m_Float);
            break;
        case eType_FCOLOR:
            node->setAttr(pParam->m_Name, Vec3(pParam->m_Value.m_Color[0], pParam->m_Value.m_Color[1], pParam->m_Value.m_Color[2]));
            break;
        case eType_VECTOR:
            node->setAttr(pParam->m_Name, Vec3(pParam->m_Value.m_Vector[0], pParam->m_Value.m_Vector[1], pParam->m_Value.m_Vector[2]));
            break;
        default:
            break;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void MaterialHelpers::MigrateXmlLegacyData(SInputShaderResources& pShaderResources, const XmlNodeRef& node) const
{
    float glowAmount;

    // Migrate glow from 3.8.3 to emittance
    if (node->getAttr("GlowAmount", glowAmount) && glowAmount > 0)
    {
        if (pShaderResources.m_Textures[EFTT_DIFFUSE].m_Sampler.m_eTexType == eTT_2D)
        {
            pShaderResources.m_Textures[EFTT_EMITTANCE].m_Name = pShaderResources.m_Textures[EFTT_DIFFUSE].m_Name;
        }

        const float legacyHDRDynMult = 2.0f;
        const float legacyIntensityScale = 10.0f;  // Legacy scale factor 10000 divided by 1000 for kilonits

        // Clamp this at EMISSIVE_INTENSITY_SOFT_MAX because some previous glow parameters become extremely bright.
        pShaderResources.m_LMaterial.m_Emittance.a = min(powf(glowAmount * legacyHDRDynMult, legacyHDRDynMult) * legacyIntensityScale, EMISSIVE_INTENSITY_SOFT_MAX);

        std::string materialName = node->getAttr("Name");
        CryWarning(VALIDATOR_MODULE_3DENGINE, VALIDATOR_WARNING, "Material %s has had legacy GlowAmount automatically converted to Emissive Intensity.  The material parameters related to Emittance should be manually adjusted for this material.", materialName.c_str());
    }
    
    // In Lumberyard version 1.9 BlendLayer2Specular became a color instead of a single float, so it needs to be updated
    XmlNodeRef publicParamsNode = node->findChild("PublicParams");
    if (publicParamsNode && publicParamsNode->haveAttr("BlendLayer2Specular"))
    {
        // Check to see if the BlendLayer2Specular is a float
        AZStd::string blendLayer2SpecularString(publicParamsNode->getAttr("BlendLayer2Specular"));

        // If there are no commas in the string representation, it must be a single float instead of a color
        if (blendLayer2SpecularString.find(',') == AZStd::string::npos)
        {
            float blendLayer2SpecularFloat = 0.0f;
            publicParamsNode->getAttr("BlendLayer2Specular", blendLayer2SpecularFloat);
            publicParamsNode->setAttr("BlendLayer2Specular", Vec4(blendLayer2SpecularFloat, blendLayer2SpecularFloat, blendLayer2SpecularFloat, 0.0));
        }
    }
}
