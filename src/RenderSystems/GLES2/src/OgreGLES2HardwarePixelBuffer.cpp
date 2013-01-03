/*
-----------------------------------------------------------------------------
This source file is part of OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2012 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/

#include "OgreRenderSystem.h"
#include "OgreGLES2HardwarePixelBuffer.h"
#include "OgreGLES2PixelFormat.h"
#include "OgreGLES2FBORenderTexture.h"
#include "OgreGLES2GpuProgram.h"
#include "OgreRoot.h"
#include "OgreGLSLESLinkProgramManager.h"
#include "OgreGLSLESLinkProgram.h"
#include "OgreGLSLESProgramPipelineManager.h"
#include "OgreGLSLESProgramPipeline.h"

static int computeLog(GLuint value)
{
    int i;

    i = 0;

    /* Error! */
    if (value == 0) return -1;

    for (;;)
    {
        if (value & 1)
        {
            /* Error! */
            if (value != 1) return -1;
                return i;
        }
        value = value >> 1;
        i++;
    }
}

namespace Ogre {
    GLES2HardwarePixelBuffer::GLES2HardwarePixelBuffer(size_t mWidth, size_t mHeight,
                                                     size_t mDepth, PixelFormat mFormat,
                                                     HardwareBuffer::Usage usage)
        : HardwarePixelBuffer(mWidth, mHeight, mDepth, mFormat, usage, false, false),
          mBuffer(mWidth, mHeight, mDepth, mFormat),
          mGLInternalFormat(GL_NONE)
    {
    }

    GLES2HardwarePixelBuffer::~GLES2HardwarePixelBuffer()
    {
        // Force free buffer
        delete [] (uint8*)mBuffer.data;
    }

    void GLES2HardwarePixelBuffer::allocateBuffer()
    {
        if (mBuffer.data)
            // Already allocated
            return;

        mBuffer.data = new uint8[mSizeInBytes];
        // TODO use PBO if we're HBU_DYNAMIC
    }

    void GLES2HardwarePixelBuffer::freeBuffer()
    {
        // Free buffer if we're STATIC to save memory
        if (mUsage & HBU_STATIC)
        {
            delete [] (uint8*)mBuffer.data;
            mBuffer.data = 0;
        }
    }

    PixelBox GLES2HardwarePixelBuffer::lockImpl(const Image::Box lockBox,  LockOptions options)
    {
        allocateBuffer();
        if (options != HardwareBuffer::HBL_DISCARD
//#ifndef GL_NV_read_buffer
            && (mUsage & HardwareBuffer::HBU_WRITE_ONLY) == 0)
        {
            // Download the old contents of the texture
            download(mBuffer);
        }
        mCurrentLockOptions = options;
        mLockedBox = lockBox;
        return mBuffer.getSubVolume(lockBox);
    }

    void GLES2HardwarePixelBuffer::unlockImpl(void)
    {
        if (mCurrentLockOptions != HardwareBuffer::HBL_READ_ONLY)
        {
            // From buffer to card, only upload if was locked for writing
            upload(mCurrentLock, mLockedBox);
        }
        freeBuffer();
    }

    void GLES2HardwarePixelBuffer::blitFromMemory(const PixelBox &src, const Image::Box &dstBox)
    {
        if (!mBuffer.contains(dstBox))
        {
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                        "Destination box out of range",
                        "GLES2HardwarePixelBuffer::blitFromMemory");
        }

        PixelBox scaled;

        if (src.getWidth() != dstBox.getWidth() ||
            src.getHeight() != dstBox.getHeight() ||
            src.getDepth() != dstBox.getDepth())
        {
            // Scale to destination size.
            // This also does pixel format conversion if needed
            allocateBuffer();
            scaled = mBuffer.getSubVolume(dstBox);
            Image::scale(src, scaled, Image::FILTER_BILINEAR);
        }
        else if (GLES2PixelUtil::getGLOriginFormat(src.format) == 0)
        {
            // Extents match, but format is not accepted as valid source format for GL
            // do conversion in temporary buffer
            allocateBuffer();
            scaled = mBuffer.getSubVolume(dstBox);
            PixelUtil::bulkPixelConversion(src, scaled);
        }
        else
        {
            allocateBuffer();
            // No scaling or conversion needed
            scaled = src;
            if (src.format == PF_R8G8B8)
            {
                scaled.format = PF_B8G8R8;
                PixelUtil::bulkPixelConversion(src, scaled);
            }
#if OGRE_PLATFORM == OGRE_PLATFORM_NACL
            if (src.format == PF_A8R8G8B8)
            {
                scaled.format = PF_A8B8G8R8;
                PixelUtil::bulkPixelConversion(src, scaled);
            }
#endif
        }

        upload(scaled, dstBox);
        freeBuffer();
    }

    void GLES2HardwarePixelBuffer::blitToMemory(const Image::Box &srcBox, const PixelBox &dst)
    {
        if (!mBuffer.contains(srcBox))
        {
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                        "source box out of range",
                        "GLES2HardwarePixelBuffer::blitToMemory");
        }

        if (srcBox.left == 0 && srcBox.right == getWidth() &&
            srcBox.top == 0 && srcBox.bottom == getHeight() &&
            srcBox.front == 0 && srcBox.back == getDepth() &&
            dst.getWidth() == getWidth() &&
            dst.getHeight() == getHeight() &&
            dst.getDepth() == getDepth() &&
            GLES2PixelUtil::getGLOriginFormat(dst.format) != 0)
        {
            // The direct case: the user wants the entire texture in a format supported by GL
            // so we don't need an intermediate buffer
            download(dst);
        }
        else
        {
            // Use buffer for intermediate copy
            allocateBuffer();
            // Download entire buffer
            download(mBuffer);
            if(srcBox.getWidth() != dst.getWidth() ||
                srcBox.getHeight() != dst.getHeight() ||
                srcBox.getDepth() != dst.getDepth())
            {
                // We need scaling
                Image::scale(mBuffer.getSubVolume(srcBox), dst, Image::FILTER_BILINEAR);
            }
            else
            {
                // Just copy the bit that we need
                PixelUtil::bulkPixelConversion(mBuffer.getSubVolume(srcBox), dst);
            }
            freeBuffer();
        }
    }

    void GLES2HardwarePixelBuffer::upload(const PixelBox &data, const Image::Box &dest)
    {
        OGRE_EXCEPT(Exception::ERR_RENDERINGAPI_ERROR,
                    "Upload not possible for this pixelbuffer type",
                    "GLES2HardwarePixelBuffer::upload");
    }

    void GLES2HardwarePixelBuffer::download(const PixelBox &data)
    {
        OGRE_EXCEPT(Exception::ERR_RENDERINGAPI_ERROR,
                    "Download not possible for this pixelbuffer type",
                    "GLES2HardwarePixelBuffer::download");
    }

    void GLES2HardwarePixelBuffer::bindToFramebuffer(GLenum attachment, size_t zoffset)
    {
        OGRE_EXCEPT(Exception::ERR_RENDERINGAPI_ERROR,
                    "Framebuffer bind not possible for this pixelbuffer type",
                    "GLES2HardwarePixelBuffer::bindToFramebuffer");
    }

    // TextureBuffer
    GLES2TextureBuffer::GLES2TextureBuffer(const String &baseName, GLenum target, GLuint id, 
                                           GLint width, GLint height, GLint internalFormat, GLint format,
                                           GLint face, GLint level, Usage usage, bool crappyCard, 
                                           bool writeGamma, uint fsaa)
    : GLES2HardwarePixelBuffer(0, 0, 0, PF_UNKNOWN, usage),
        mTarget(target), mTextureID(id), mFace(face), mLevel(level), mSoftwareMipmap(crappyCard)
    {
        GL_CHECK_ERROR;

        glBindTexture(mTarget, mTextureID);
        GL_CHECK_ERROR;

        // Get face identifier
        mFaceTarget = mTarget;
        if(mTarget == GL_TEXTURE_CUBE_MAP)
            mFaceTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;

        // Calculate the width and height of the texture at this mip level
        mWidth = mLevel == 0 ? width : width / static_cast<size_t>(pow(2.0f, level));
        mHeight = mLevel == 0 ? height : height / static_cast<size_t>(pow(2.0f, level));
        if(mWidth < 1)
            mWidth = 1;
        if(mHeight < 1)
            mHeight = 1;

        // Only 2D is supported so depth is always 1
        mDepth = 1;

        mGLInternalFormat = internalFormat;
        mFormat = GLES2PixelUtil::getClosestOGREFormat(internalFormat, format);

        mRowPitch = mWidth;
        mSlicePitch = mHeight*mWidth;
        mSizeInBytes = PixelUtil::getMemorySize(mWidth, mHeight, mDepth, mFormat);

        // Log a message
//        std::stringstream str;
//        str << "GLES2HardwarePixelBuffer constructed for texture " << baseName
//            << " id " << mTextureID << " face " << mFace << " level " << mLevel << ": "
//            << "width=" << mWidth << " height="<< mHeight << " depth=" << mDepth
//            << " format=" << PixelUtil::getFormatName(mFormat);
//        LogManager::getSingleton().logMessage(LML_NORMAL, str.str());

        // Set up a pixel box
        mBuffer = PixelBox(mWidth, mHeight, mDepth, mFormat);
        
        if (mWidth==0 || mHeight==0 || mDepth==0)
            // We are invalid, do not allocate a buffer
            return;

        // Is this a render target?
        if (mUsage & TU_RENDERTARGET)
        {
            // Create render target for each slice
            mSliceTRT.reserve(mDepth);
            for(size_t zoffset=0; zoffset<mDepth; ++zoffset)
            {
                String name;
                name = "rtt/" + StringConverter::toString((size_t)this) + "/" + baseName;
                GLES2SurfaceDesc surface;
                surface.buffer = this;
                surface.zoffset = zoffset;
                RenderTexture *trt = GLES2RTTManager::getSingleton().createRenderTexture(name, surface, writeGamma, fsaa);
                mSliceTRT.push_back(trt);
                Root::getSingleton().getRenderSystem()->attachRenderTarget(*mSliceTRT[zoffset]);
            }
        }
    }

    GLES2TextureBuffer::~GLES2TextureBuffer()
    {
        if (mUsage & TU_RENDERTARGET)
        {
            // Delete all render targets that are not yet deleted via _clearSliceRTT because the rendertarget
            // was deleted by the user.
            for (SliceTRT::const_iterator it = mSliceTRT.begin(); it != mSliceTRT.end(); ++it)
            {
                Root::getSingleton().getRenderSystem()->destroyRenderTarget((*it)->getName());
            }
        }
    }

    void GLES2TextureBuffer::upload(const PixelBox &data, const Image::Box &dest)
    {
        glBindTexture(mTarget, mTextureID);
        GL_CHECK_ERROR;

        if (PixelUtil::isCompressed(data.format))
        {
            if(data.format != mFormat || !data.isConsecutive())
                OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                            "Compressed images must be consecutive, in the source format",
                            "GLES2TextureBuffer::upload");

            GLenum format = GLES2PixelUtil::getClosestGLInternalFormat(mFormat);
            // Data must be consecutive and at beginning of buffer as PixelStorei not allowed
            // for compressed formats
            if (dest.left == 0 && dest.top == 0)
            {
                glCompressedTexImage2D(mFaceTarget, mLevel,
                                       format,
                                       dest.getWidth(),
                                       dest.getHeight(),
                                       0,
                                       data.getConsecutiveSize(),
                                       data.data);
                GL_CHECK_ERROR;
            }
            else
            {
                glCompressedTexSubImage2D(mFaceTarget, mLevel,
                                          dest.left, dest.top,
                                          dest.getWidth(), dest.getHeight(),
                                          format, data.getConsecutiveSize(),
                                          data.data);

                GL_CHECK_ERROR;
            }
        }
        else if (mSoftwareMipmap)
        {
            if (data.getWidth() != data.rowPitch)
            {
                // TODO
                OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                            "Unsupported texture format",
                            "GLES2TextureBuffer::upload");
            }

            if (data.getHeight() * data.getWidth() != data.slicePitch)
            {
                // TODO
                OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                            "Unsupported texture format",
                            "GLES2TextureBuffer::upload");
            }

            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            GL_CHECK_ERROR;
            buildMipmaps(data);
        }
        else
        {
            if(data.getWidth() != data.rowPitch)
            {
                // TODO
                OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                            "Unsupported texture format",
                            "GLES2TextureBuffer::upload");
            }

            if(data.getHeight()*data.getWidth() != data.slicePitch)
            {
                // TODO
                OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                    "Unsupported texture format",
                    "GLES2TextureBuffer::upload");
            }

            if ((data.getWidth() * PixelUtil::getNumElemBytes(data.format)) & 3) {
                // Standard alignment of 4 is not right
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                GL_CHECK_ERROR;
            }

//            LogManager::getSingleton().logMessage("GLES2TextureBuffer::upload - ID: " + StringConverter::toString(mTextureID) +
//                                                  " Format: " + PixelUtil::getFormatName(data.format) +
//                                                  " Origin format: " + StringConverter::toString(GLES2PixelUtil::getGLOriginFormat(data.format), 0, std::ios::hex) +
//                                                  " Data type: " + StringConverter::toString(GLES2PixelUtil::getGLOriginDataType(data.format), 0, ' ', std::ios::hex));
            glTexSubImage2D(mFaceTarget,
                            mLevel,
                            dest.left, dest.top,
                            dest.getWidth(), dest.getHeight(),
                            GLES2PixelUtil::getGLOriginFormat(data.format),
                            GLES2PixelUtil::getGLOriginDataType(data.format),
                            data.data);
        }
        
        if ((mUsage & TU_AUTOMIPMAP) && !mSoftwareMipmap && (mLevel == 0))
        {
            glGenerateMipmap(mFaceTarget);
            GL_CHECK_ERROR;
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        GL_CHECK_ERROR;
    }

    //-----------------------------------------------------------------------------  
    void GLES2TextureBuffer::download(const PixelBox &data)
    {
#if 0 || defined(GL_NV_get_tex_image)
        if(data.getWidth() != getWidth() ||
            data.getHeight() != getHeight() ||
            data.getDepth() != getDepth())
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, "only download of entire buffer is supported by GL",
                "GLTextureBuffer::download");
        glBindTexture( mTarget, mTextureID );
        if(PixelUtil::isCompressed(data.format))
        {
            if(data.format != mFormat || !data.isConsecutive())
                OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, 
                "Compressed images must be consecutive, in the source format",
                "GLTextureBuffer::download");
            // Data must be consecutive and at beginning of buffer as PixelStorei not allowed
            // for compressed formate
            glGetCompressedTexImageNV(mFaceTarget, mLevel, data.data);
        } 
        else
        {
            if((data.getWidth()*PixelUtil::getNumElemBytes(data.format)) & 3) {
                // Standard alignment of 4 is not right
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
            }
            // We can only get the entire texture
            glGetTexImageNV(mFaceTarget, mLevel, 
                GLES2PixelUtil::getGLOriginFormat(data.format), GLES2PixelUtil::getGLOriginDataType(data.format),
                data.data);
            // Restore defaults
            glPixelStorei(GL_PACK_ALIGNMENT, 4);
        }
#else
        OGRE_EXCEPT(Exception::ERR_NOT_IMPLEMENTED, 
                    "Downloading texture buffers is not supported by OpenGL ES",
                    "GLES2TextureBuffer::download");
#endif
    }
    //-----------------------------------------------------------------------------  
    void GLES2TextureBuffer::bindToFramebuffer(GLenum attachment, size_t zoffset)
    {
        assert(zoffset < mDepth);
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                                  mFaceTarget, mTextureID, mLevel);
        GL_CHECK_ERROR;
    }
    
    void GLES2TextureBuffer::copyFromFramebuffer(size_t zoffset)
    {
        glBindTexture(mTarget, mTextureID);
        GL_CHECK_ERROR;
        glCopyTexSubImage2D(mFaceTarget, mLevel, 0, 0, 0, 0, mWidth, mHeight);
        GL_CHECK_ERROR;
    }

    //-----------------------------------------------------------------------------  
    void GLES2TextureBuffer::blit(const HardwarePixelBufferSharedPtr &src, const Image::Box &srcBox, const Image::Box &dstBox)
    {
        GLES2TextureBuffer *srct = static_cast<GLES2TextureBuffer *>(src.getPointer());
        // TODO: Check for FBO support first
        // Destination texture must be 2D or Cube
        // Source texture must be 2D
        if((src->getUsage() & TU_RENDERTARGET) == 0 && (srct->mTarget == GL_TEXTURE_2D))
        {
            blitFromTexture(srct, srcBox, dstBox);
        }
        else
        {
            GLES2HardwarePixelBuffer::blit(src, srcBox, dstBox);
        }
    }
    
    //-----------------------------------------------------------------------------  
    // Very fast texture-to-texture blitter and hardware bi/trilinear scaling implementation using FBO
    // Destination texture must be 1D, 2D, 3D, or Cube
    // Source texture must be 1D, 2D or 3D
    // Supports compressed formats as both source and destination format, it will use the hardware DXT compressor
    // if available.
    // @author W.J. van der Laan
    void GLES2TextureBuffer::blitFromTexture(GLES2TextureBuffer *src, const Image::Box &srcBox, const Image::Box &dstBox)
    {
		return; // todo - add a shader attach...
//        std::cerr << "GLES2TextureBuffer::blitFromTexture " <<
//        src->mTextureID << ":" << srcBox.left << "," << srcBox.top << "," << srcBox.right << "," << srcBox.bottom << " " << 
//        mTextureID << ":" << dstBox.left << "," << dstBox.top << "," << dstBox.right << "," << dstBox.bottom << std::endl;

        // Store reference to FBO manager
        GLES2FBOManager *fboMan = static_cast<GLES2FBOManager *>(GLES2RTTManager::getSingletonPtr());
        
        RenderSystem* rsys = Root::getSingleton().getRenderSystem();
        rsys->_disableTextureUnitsFrom(0);
		glActiveTexture(GL_TEXTURE0);

        // Disable alpha, depth and scissor testing, disable blending, 
        // and disable culling
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);

        // Set up source texture
        glBindTexture(src->mTarget, src->mTextureID);
        GL_CHECK_ERROR;
        
        // Set filtering modes depending on the dimensions and source
        if(srcBox.getWidth()==dstBox.getWidth() &&
           srcBox.getHeight()==dstBox.getHeight() &&
           srcBox.getDepth()==dstBox.getDepth())
        {
            // Dimensions match -- use nearest filtering (fastest and pixel correct)
            glTexParameteri(src->mTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            GL_CHECK_ERROR;
            glTexParameteri(src->mTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            GL_CHECK_ERROR;
        }
        else
        {
            // Dimensions don't match -- use bi or trilinear filtering depending on the
            // source texture.
            if(src->mUsage & TU_AUTOMIPMAP)
            {
                // Automatic mipmaps, we can safely use trilinear filter which
                // brings greatly improved quality for minimisation.
                glTexParameteri(src->mTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                GL_CHECK_ERROR;
                glTexParameteri(src->mTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);    
                GL_CHECK_ERROR;
            }
            else
            {
                // Manual mipmaps, stay safe with bilinear filtering so that no
                // intermipmap leakage occurs.
                glTexParameteri(src->mTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                GL_CHECK_ERROR;
                glTexParameteri(src->mTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                GL_CHECK_ERROR;
            }
        }
        // Clamp to edge (fastest)
        glTexParameteri(src->mTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        GL_CHECK_ERROR;
        glTexParameteri(src->mTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        GL_CHECK_ERROR;
        
        // Store old binding so it can be restored later
        GLint oldfb;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldfb);
        GL_CHECK_ERROR;

        // Set up temporary FBO
        glBindFramebuffer(GL_FRAMEBUFFER, fboMan->getTemporaryFBO());
        GL_CHECK_ERROR;

        GLuint tempTex = 0;
        if(!fboMan->checkFormat(mFormat))
        {
            // If target format not directly supported, create intermediate texture
            GLenum tempFormat = GLES2PixelUtil::getClosestGLInternalFormat(fboMan->getSupportedAlternative(mFormat));
            glGenTextures(1, &tempTex);
            GL_CHECK_ERROR;
            glBindTexture(GL_TEXTURE_2D, tempTex);
            GL_CHECK_ERROR;
#if GL_APPLE_texture_max_level && OGRE_PLATFORM != OGRE_PLATFORM_NACL
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL_APPLE, 0);
            GL_CHECK_ERROR;
#endif
            // Allocate temporary texture of the size of the destination area
            glTexImage2D(GL_TEXTURE_2D, 0, tempFormat, 
                         GLES2PixelUtil::optionalPO2(dstBox.getWidth()), GLES2PixelUtil::optionalPO2(dstBox.getHeight()), 
                         0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
            GL_CHECK_ERROR;
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      GL_TEXTURE_2D, tempTex, 0);
            GL_CHECK_ERROR;
            // Set viewport to size of destination slice
            glViewport(0, 0, dstBox.getWidth(), dstBox.getHeight());
            GL_CHECK_ERROR;
        }
        else
        {
            // We are going to bind directly, so set viewport to size and position of destination slice
            glViewport(dstBox.left, dstBox.top, dstBox.getWidth(), dstBox.getHeight());
            GL_CHECK_ERROR;
        }
        
        // Process each destination slice
        for(size_t slice=dstBox.front; slice<dstBox.back; ++slice)
        {
            if(!tempTex)
            {
                // Bind directly
                bindToFramebuffer(GL_COLOR_ATTACHMENT0, slice);
            }

            /// Calculate source texture coordinates
            float u1 = (float)srcBox.left / (float)src->mWidth;
            float v1 = (float)srcBox.top / (float)src->mHeight;
            float u2 = (float)srcBox.right / (float)src->mWidth;
            float v2 = (float)srcBox.bottom / (float)src->mHeight;
            /// Calculate source slice for this destination slice
            float w = (float)(slice - dstBox.front) / (float)dstBox.getDepth();
            /// Get slice # in source
            w = w * (float)srcBox.getDepth() + srcBox.front;
            /// Normalise to texture coordinate in 0.0 .. 1.0
            w = (w+0.5f) / (float)src->mDepth;
            
            /// Finally we're ready to rumble	
            glBindTexture(src->mTarget, src->mTextureID);
            GL_CHECK_ERROR;
            glEnable(src->mTarget);
            GL_CHECK_ERROR;

            GLfloat squareVertices[] = {
               -1.0f, -1.0f,
                1.0f, -1.0f,
               -1.0f,  1.0f,
                1.0f,  1.0f,
            };
            GLfloat texCoords[] = {
                u1, v1, w,
                u2, v1, w,
                u2, v2, w,
                u1, v2, w
            };

            GLuint posAttrIndex = 0;
            GLuint texAttrIndex = 0;
            if(Root::getSingleton().getRenderSystem()->getCapabilities()->hasCapability(RSC_SEPARATE_SHADER_OBJECTS))
            {
                GLSLESProgramPipeline* programPipeline = GLSLESProgramPipelineManager::getSingleton().getActiveProgramPipeline();
                posAttrIndex = (GLuint)programPipeline->getAttributeIndex(VES_POSITION, 0);
                texAttrIndex = (GLuint)programPipeline->getAttributeIndex(VES_TEXTURE_COORDINATES, 0);
            }
            else
            {
                GLSLESLinkProgram* linkProgram = GLSLESLinkProgramManager::getSingleton().getActiveLinkProgram();
                posAttrIndex = (GLuint)linkProgram->getAttributeIndex(VES_POSITION, 0);
                texAttrIndex = (GLuint)linkProgram->getAttributeIndex(VES_TEXTURE_COORDINATES, 0);
            }

            // Draw the textured quad
            glVertexAttribPointer(posAttrIndex,
                                  2,
                                  GL_FLOAT,
                                  0,
                                  0,
                                  squareVertices);
            GL_CHECK_ERROR;
            glEnableVertexAttribArray(posAttrIndex);
            GL_CHECK_ERROR;
            glVertexAttribPointer(texAttrIndex,
                                  3,
                                  GL_FLOAT,
                                  0,
                                  0,
                                  texCoords);
            GL_CHECK_ERROR;
            glEnableVertexAttribArray(texAttrIndex);
            GL_CHECK_ERROR;

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            GL_CHECK_ERROR;

            glDisable(src->mTarget);
            GL_CHECK_ERROR;

            if(tempTex)
            {
                // Copy temporary texture
                glBindTexture(mTarget, mTextureID);
                GL_CHECK_ERROR;
                switch(mTarget)
                {
                    case GL_TEXTURE_2D:
                    case GL_TEXTURE_CUBE_MAP:
                        glCopyTexSubImage2D(mFaceTarget, mLevel, 
                                            dstBox.left, dstBox.top, 
                                            0, 0, dstBox.getWidth(), dstBox.getHeight());
                        GL_CHECK_ERROR;
                        break;
                }
            }
        }
        // Finish up 
        if(!tempTex)
        {
            // Generate mipmaps
            if(mUsage & TU_AUTOMIPMAP)
            {
                glBindTexture(mTarget, mTextureID);
                GL_CHECK_ERROR;
                LogManager::getSingleton().logMessage("GLES2TextureBuffer::blitFromTexture, glGenerateMipmap(mTarget);");

                glGenerateMipmap(mTarget);
                GL_CHECK_ERROR;
            }
        }
        
        // Reset source texture to sane state
        glBindTexture(src->mTarget, src->mTextureID);
        GL_CHECK_ERROR;
        
        // Detach texture from temporary framebuffer
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_RENDERBUFFER, 0);
        GL_CHECK_ERROR;
        // Restore old framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, oldfb);
        GL_CHECK_ERROR;
        glDeleteTextures(1, &tempTex);
        GL_CHECK_ERROR;
    }
    //-----------------------------------------------------------------------------  
    // blitFromMemory doing hardware trilinear scaling
    void GLES2TextureBuffer::blitFromMemory(const PixelBox &src_orig, const Image::Box &dstBox)
    {
        // Fall back to normal GLHardwarePixelBuffer::blitFromMemory in case 
        // - FBO is not supported
        // - Either source or target is luminance due doesn't looks like supported by hardware
        // - the source dimensions match the destination ones, in which case no scaling is needed
        // TODO: Check that extension is NOT available
        if(PixelUtil::isLuminance(src_orig.format) ||
           PixelUtil::isLuminance(mFormat) ||
           (src_orig.getWidth() == dstBox.getWidth() &&
            src_orig.getHeight() == dstBox.getHeight() &&
            src_orig.getDepth() == dstBox.getDepth()))
        {
            GLES2HardwarePixelBuffer::blitFromMemory(src_orig, dstBox);
            return;
        }
        if(!mBuffer.contains(dstBox))
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, "Destination box out of range",
                        "GLES2TextureBuffer::blitFromMemory");
        // For scoped deletion of conversion buffer
        MemoryDataStreamPtr buf;
        PixelBox src;
        
        // First, convert the srcbox to a OpenGL compatible pixel format
        if(GLES2PixelUtil::getGLOriginFormat(src_orig.format) == 0)
        {
            // Convert to buffer internal format
            buf.bind(OGRE_NEW MemoryDataStream(PixelUtil::getMemorySize(src_orig.getWidth(), src_orig.getHeight(),
                                                                        src_orig.getDepth(), mFormat)));
            src = PixelBox(src_orig.getWidth(), src_orig.getHeight(), src_orig.getDepth(), mFormat, buf->getPtr());
            PixelUtil::bulkPixelConversion(src_orig, src);
        }
        else
        {
            // No conversion needed
            src = src_orig;
        }
        
        // Create temporary texture to store source data
        GLuint id;
        GLenum target = GL_TEXTURE_2D;
        GLsizei width = GLES2PixelUtil::optionalPO2(src.getWidth());
        GLsizei height = GLES2PixelUtil::optionalPO2(src.getHeight());
        GLenum format = GLES2PixelUtil::getClosestGLInternalFormat(src.format);
        GLenum datatype = GLES2PixelUtil::getGLOriginDataType(src.format);

        // Generate texture name
        glGenTextures(1, &id);
        GL_CHECK_ERROR;
        
        // Set texture type
        glBindTexture(target, id);
        GL_CHECK_ERROR;

#if GL_APPLE_texture_max_level && OGRE_PLATFORM != OGRE_PLATFORM_NACL && OGRE_PLATFORM != OGRE_PLATFORM_BLACKBERRY
        glTexParameteri(target, GL_TEXTURE_MAX_LEVEL_APPLE, 1000 );
        GL_CHECK_ERROR;
#endif

        // Allocate texture memory
        glTexImage2D(target, 0, format, width, height, 0, format, datatype, 0);
        GL_CHECK_ERROR;

        // GL texture buffer
        GLES2TextureBuffer tex(StringUtil::BLANK, target, id, width, height, format, src.format,
                              0, 0, (Usage)(TU_AUTOMIPMAP|HBU_STATIC_WRITE_ONLY), false, false, 0);
        
        // Upload data to 0,0,0 in temporary texture
        Image::Box tempTarget(0, 0, 0, src.getWidth(), src.getHeight(), src.getDepth());
        tex.upload(src, tempTarget);
        
        // Blit
        blitFromTexture(&tex, tempTarget, dstBox);
        
        // Delete temp texture
        glDeleteTextures(1, &id);
        GL_CHECK_ERROR;
    }
    
    RenderTexture *GLES2TextureBuffer::getRenderTarget(size_t zoffset)
    {
        assert(mUsage & TU_RENDERTARGET);
        assert(zoffset < mDepth);
        return mSliceTRT[zoffset];
    }

    void GLES2TextureBuffer::buildMipmaps(const PixelBox &data)
    {
        int width;
        int height;
        int logW;
        int logH;
        int level;
        PixelBox scaled = data;
        scaled.data = data.data;
        scaled.left = data.left;
        scaled.right = data.right;
        scaled.top = data.top;
        scaled.bottom = data.bottom;
        scaled.front = data.front;
        scaled.back = data.back;

        width = data.getWidth();
        height = data.getHeight();

        logW = computeLog(width);
        logH = computeLog(height);
        level = (logW > logH ? logW : logH);

        for (int mip = 0; mip <= level; mip++)
        {
            GLenum glFormat = GLES2PixelUtil::getGLOriginFormat(scaled.format);
            GLenum dataType = GLES2PixelUtil::getGLOriginDataType(scaled.format);

            glTexImage2D(mFaceTarget,
                         mip,
                         glFormat,
                         width, height,
                         0,
                         glFormat,
                         dataType,
                         scaled.data);

            GL_CHECK_ERROR;

            if (mip != 0)
            {
                OGRE_DELETE[] (uint8*) scaled.data;
                scaled.data = 0;
            }

            if (width > 1)
            {
                width = width / 2;
            }

            if (height > 1)
            {
                height = height / 2;
            }

            int sizeInBytes = PixelUtil::getMemorySize(width, height, 1,
                                                       data.format);
            scaled = PixelBox(width, height, 1, data.format);
            scaled.data = new uint8[sizeInBytes];
            Image::scale(data, scaled, Image::FILTER_LINEAR);
        }

        // Delete the scaled data for the last level
        if (level > 0)
        {
            delete[] (uint8*) scaled.data;
            scaled.data = 0;
        }
    }
    
    //********* GLES2RenderBuffer
    //----------------------------------------------------------------------------- 
    GLES2RenderBuffer::GLES2RenderBuffer(GLenum format, size_t width, size_t height, GLsizei numSamples):
    GLES2HardwarePixelBuffer(width, height, 1, GLES2PixelUtil::getClosestOGREFormat(format, PF_A8R8G8B8),HBU_WRITE_ONLY)
    {
        mGLInternalFormat = format;
        // Generate renderbuffer
        glGenRenderbuffers(1, &mRenderbufferID);
        GL_CHECK_ERROR;
        // Bind it to FBO
        glBindRenderbuffer(GL_RENDERBUFFER, mRenderbufferID);
        GL_CHECK_ERROR;
        
        // Allocate storage for depth buffer
        if (numSamples > 0)
        {
#if GL_APPLE_framebuffer_multisample && OGRE_PLATFORM != OGRE_PLATFORM_NACL&& OGRE_PLATFORM != OGRE_PLATFORM_BLACKBERRY  && OGRE_PLATFORM != OGRE_PLATFORM_WIN32
            glRenderbufferStorageMultisampleAPPLE(GL_RENDERBUFFER, 
                                                numSamples, format, width, height);
            GL_CHECK_ERROR;
#endif
        }
        else
        {
            glRenderbufferStorage(GL_RENDERBUFFER, format,
                                     width, height);
            GL_CHECK_ERROR;
        }
    }
    //----------------------------------------------------------------------------- 
    GLES2RenderBuffer::~GLES2RenderBuffer()
    {
        // Generate renderbuffer
        glDeleteRenderbuffers(1, &mRenderbufferID);
        GL_CHECK_ERROR;
    }
    //-----------------------------------------------------------------------------  
    void GLES2RenderBuffer::bindToFramebuffer(GLenum attachment, size_t zoffset)
    {
        assert(zoffset < mDepth);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment,
                                     GL_RENDERBUFFER, mRenderbufferID);
        GL_CHECK_ERROR;
    }
}
