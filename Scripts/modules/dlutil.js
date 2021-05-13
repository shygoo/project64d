module.exports = {
    DLParser: DLParser
};

function loadMatrix(ram, offset, dst)
{
    for(var i = 0; i < 16; i++) {
        dst[i] = ((ram.readInt16BE(offset + i * 2) << 16) | ram.readUInt16BE(offset + 32 + i * 2)) / 0x10000;
    }
}

function DLParser()
{
    this.ram = null;
    this.result = {};
}

DLParser.prototype.parseF3D = function(dlistAddr, screenWidth, screenHeight) {
    var parseTimeStart = performance.now();

    var dlAddress = dlistAddr;
    var ram = this.ram;

    var w0, w1;
    
    var spVertices = new Array(16);
    var spJumpStack = new Uint32Array(16);
    var spSegments = new Uint32Array(16);
    var spJumpStackIndex = 0;
    var spMatrices = new Array(16);
    var spMatrixIndex = 0;
    var spProjectionMatrix = mat4.create();
    var spFinalMatrix = mat4.create();

    var screenMtx = mat4.create();
    screenMtx[0] = screenWidth/2;
    screenMtx[5] = -screenHeight/2;
    screenMtx[12] = screenWidth/2;
    screenMtx[13] = screenHeight/2;

    var scratchMtx = mat4.create();

    for(var i = 0; i < 16; i++) {
        spSegments[i] = 0;
    }

    for(var i = 0; i < 64; i++) {
        // (only loading xyz)
        spVertices[i] = vec3.create();
    }

    for(var i = 0; i < spMatrices.length; i++) {
        spMatrices[i] = mat4.create();
    }

    // G_VTX
    var numv, vidx, vaddr, vend, nv, curv,
        ix, iy, iz, ox, oy, oz, ow;

    // G_TRI
    var v0, v1, v2;

    // G_MTX
    var mtxflags;

    // (this reduces call time)
    const getu32 = ram.readUInt32BE.bind(ram);
    const setu32 = ram.writeUInt32BE.bind(ram);
    const gets16 = ram.readInt16BE.bind(ram);
    const vec3_transformMat4 = vec3.transformMat4;
    const mat4_mul = mat4.mul;

    // result
    var numCommands = 0;
    var numTriangles = 0;
    var numVertices = 0;
    var vertexHeap = new Array(5000);
    var vhidx = 0;

    var faceHeap = [];
    var faceIdx = 0;

    for(var done = false; !done;) {
        w0 = getu32(dlAddress);
        w1 = getu32(dlAddress + 4);
        dlAddress += 8;
        
        switch(w0 >>> 24)
        {
            case 0x04: {
                numv = ((w0 >> 20) & 0x0F) + 1;
                vidx = (w0 >> 16) & 0x0F;
                vaddr = spSegments[(w1 >>> 24) & 0x0F] + (w1 & 0x00FFFFFF);
                vend = vaddr + numv * 16;
                nv = 0;

                while(vaddr < vend) {
                    curv = spVertices[vidx + nv];
                    curv[0] = gets16(vaddr);
                    curv[1] = gets16(vaddr + 2);
                    curv[2] = gets16(vaddr+ 4);
                    vec3_transformMat4(curv, curv, spFinalMatrix);
                    curv.debugAddr = K0BASE + vaddr;
                    vaddr += 16;
                    nv++;
                }
                //numVertices += numv;
            } break;
            case 0xBF: {
                v0 = spVertices[((w1 >>> 16) & 0xFF) / 0x0A];
                v1 = spVertices[((w1 >>>  8) & 0xFF) / 0x0A];
                v2 = spVertices[((w1 >>>  0) & 0xFF) / 0x0A];

                var x0 = v0[0], y0 = v0[1], z0 = v0[2];
                var x1 = v1[0], y1 = v1[1], z1 = v1[2];
                var x2 = v2[0], y2 = v2[1], z2 = v2[2];

                // todo test w
                if(z0 > 1 || z1 > 1 || z2 > 1)
                    break;

                if(x0 < 0 && x1 < 0 && x2 < 0) continue;
                if(x0 >= 640 && x1 >= 640 && x2 >= 640) continue;
                if(y0 < 0 && y1 < 0 && y2 < 0) continue;
                if(y0 >= 480 && y1 >= 480 && y2 >= 480) continue;

                vertexHeap[vhidx] = [ x0, y0, z0, v0.debugAddr ];
                vertexHeap[vhidx+1] = [ x1, y1, z1, v1.debugAddr ];
                vertexHeap[vhidx+2] = [ x2, y2, z2, v2.debugAddr ];
                faceHeap[faceIdx] = [vhidx, vhidx + 1, vhidx + 2];
                faceHeap[faceIdx].debugAddr = K0BASE + dlAddress - 8;
                vhidx += 3;
                faceIdx++;
                //numTriangles++;
            } break;
            case 0x01: {
                var maddr = spSegments[(w1 >>> 24) & 0x0F] + (w1 & 0x00FFFFFF);
                mtxflags = (w0 >>> 16) & 0xFF;
                
                if(mtxflags & 0x01) { // G_MTX_PROJECTION
                    if(mtxflags & 0x02) { // G_MTX_LOAD
                        loadMatrix(ram, maddr, spProjectionMatrix);
                    }
                    else {
                        loadMatrix(ram, maddr, scratchMtx);
                        mat4_mul(spProjectionMatrix, spProjectionMatrix, scratchMtx);
                    }
                }
                else { // G_MTX_MODELVIEW
                    var curMtx = spMatrices[spMatrixIndex];

                    if(mtxflags & 0x04) { // G_MTX_PUSH
                        spMatrixIndex++;
                    }

                    if(mtxflags & 0x02) { // G_MTX_LOAD
                        loadMatrix(ram, maddr, spMatrices[spMatrixIndex]);
                    }
                    else { // G_MTX_MUL
                        loadMatrix(ram, maddr, scratchMtx);
                        mat4_mul(spMatrices[spMatrixIndex], curMtx, scratchMtx);
                    }

                    mat4_mul(spFinalMatrix, spMatrices[spMatrixIndex], spProjectionMatrix);
                    mat4_mul(spFinalMatrix, spFinalMatrix, screenMtx);
                }
            } break;
            case 0xBD: {
                spMatrixIndex--;
            } break;
            case 0x06: {
                if((w0 & 0x00FF0000) >>> 16 != 0x01) {
                    spJumpStack[spJumpStackIndex++] = dlAddress;
                }
                dlAddress = spSegments[(w1 >>> 24) & 0x0F] + (w1 & 0x00FFFFFF);
            } break;
            case 0xB8: {
                if(spJumpStackIndex > 0) {
                    dlAddress = spJumpStack[--spJumpStackIndex];
                }
                else {
                    done = true;
                }
            } break;
            case 0xBC: {
                if((w0 & 0xFF) == 0x06) {
                    spSegments[((w0 & 0xFF00) >>> 8) / 4] = w1 & 0x1FFFFFFF;
                }
            } break;
        }
        //numCommands++;
    }

    this.result.parseTime = performance.now() - parseTimeStart;
    this.result.numCommands = numCommands;
    this.result.numTriangles = numTriangles;
    this.result.numVertices = numVertices;
    this.result.faceHeap = faceHeap;
    this.result.vertexHeap = vertexHeap;
}
