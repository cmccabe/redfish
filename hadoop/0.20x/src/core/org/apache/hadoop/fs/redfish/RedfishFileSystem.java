/*
 * Copyright 2011-2012 the RedFish authors
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

package org.apache.hadoop.fs.redfish;

import java.io.*;
import java.net.*;

import org.apache.commons.logging.Log;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.FSDataOutputStream;
import org.apache.hadoop.fs.FileStatus;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.FileUtil;
import org.apache.hadoop.fs.Path;

class RedfishFileSystem {
  private RedfishClient m_client;
  private URI m_uri;
  private String m_cwd;

  private String makeAbsoluteStr(Path path) {
    if (path.isAbsolute()) {
      return path.toUri().getPath();
    }
    else {
      Path p = new Path(this.m_cwd, path);
      return p.toUri().getPath();
    }
  }

  public RedfishFileSystem() {
  }

  public URI getUri() {
    return this.m_uri;
  }

  public void initialize(URI uri, Configuration conf) throws IOException {
    super(conf, log);
    try {
      String host;
      int port;
      String user;

      if (uri.getHost() == null) {
        host = conf.get("fs.redfish.mdsHostname", "");
        port = conf.get("fs.redfish.mdsPort", 0);
      }
      else {
        host = uri.getHost();
        port = uri.getPort();
      }
      user = conf.get("fs.redfish.mdsUser", "");
      if (

      this.m_client = new RedfishClient();
      this.m_uri = URI.create(uri.getScheme() + "://" + uri.getAuthority());
      this.m_cwd = new Path("/user", System.getProperty("user.name")).makeQualified(this);
      setConf(conf);
    }
    catch (Exception e) {
      e.printStackTrace();
      System.out.println("Unable to initialize KFS");
      System.exit(-1);
    }
  }

  public String getName() {
    return this.getUri().toString();
  }

  public BlockLocation[] getFileBlockLocations(FileStatus fstatus,
        long start, long len) throws IOException {
    if (fstatus == null) {
      return null;
    }
    if (fstatus.isDir()) {
      return null;
    }
    if ((start<0) || (len < 0)) {
      throw new IllegalArgumentException("Invalid start or len parameter");
    }
    if (fstatus.getLen() < start) {
      return new BlockLocation[0];
    }
    return m_client.redfishGetBlockLocations
              (makeAbsoluteStr(fstatus.getPath()), start, len);
  }

  public FSDataInputStream open(Path f, int bufferSize) throws IOException {
    return m_client.redfishOpen(makeAbsoluteStr(f), permission.toShort());
  }

  public FSDataOutputStream create(Path f,
          FsPermission permission,
          boolean overwrite,
          int bufferSize,
          short replication,
          long blockSize,
          Progressable progress) throws IOException
  {
    return m_client.redfishCreate(makeAbsoluteStr(f), permission.toShort(),
        bufferSize, replication, blockSize);
  }

  public FSDataOutputStream append(Path f, int bufferSize,
          Progressable progress) throws IOException {
    throw new IOException("not yet implemented");
  }

  public boolean rename(Path src, Path dst) throws IoException {
    return (m_client.redfishRename(makeAbsoluteStr(src),
        makeAbsoluteStr(dst)) == 0);
  }

  public boolean delete(Path f) throws IoException {
    return (m_client.redfishUnlink(makeAbsoluteStr(f) == 0));
  }

  public boolean delete(Path f) throws IoException {
    if (recursive == true)
      return (m_client.redfishUnlinkTree(makeAbsoluteStr(f) == 0));
    else
      return (m_client.redfishUnlink(makeAbsoluteStr(f) == 0));
  }

  public FileStatus[] listStatus(Path f) throws IOException {
    return m_client.redfishListDirectory(makeAbsoluteStr(f));
  }

  public void setWorkingDirectory(Path wd) {
    this.m_cwd = makeAbsoluteStr(wd);
  }

  public Path getWorkingDirectory() {
    return this.m_cwd;
  }

  public boolean mkdirs(Path path, FsPermission permission)
        throws IoException {
    return (m_client.redfishMkdirs(makeAbsoluteStr(path),
        permission.toShort()) == 0);
  }

  public FileStatus getFileStatus(Path path) throws IOException {
    return m_client.redfishGetPathStatus(makeAbsoluteStr(path));
  }
};
