/*
 * Copyright 2011-2012 the Redfish authors
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

class RedfishFileSystem extends FileSystem {
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
    String configFile = conf.get("fs.redfish.configFile", "");
    if (configFile == "") {
      throw new IOException("You must set fs.redfish.configFile to the " +
          "path to a valid Redfish configuration file");
    }
    String userName = System.getProperty("user.name");
    this.m_client = new RedfishClient(configFile, userName);
    this.m_uri = URI.create(uri.getScheme() + "://" + uri.getAuthority());
    this.m_cwd = new Path("/user", System.getProperty("user.name")).makeQualified(this);
    setConf(conf);
  }

  /* This will free most of the resources associated with the RedfishFileSystem.
   */
  public void close() throws IOException {
    super();
    m_client.redfishDisconnect();
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
    return m_client.redfishOpen(makeAbsoluteStr(f));
  }

  public FSDataOutputStream create(Path f,
          FsPermission perm,
          boolean overwrite,
          int bufferSize,
          short replication,
          long blockSize,
          Progressable progress) throws IOException
  {
    return m_client.redfishCreate(makeAbsoluteStr(f), perm.toShort(),
                bufferSize, replication, blockSize);
  }

  public FSDataOutputStream append(Path f, int bufferSize,
          Progressable progress) throws IOException {
    throw new IOException("not yet implemented");
  }

  public boolean rename(Path src, Path dst) throws IoException {
    return m_client.redfishRename(makeAbsoluteStr(src), makeAbsoluteStr(dst));
  }

  public boolean delete(Path f) throws IoException {
    return m_client.redfishUnlink(makeAbsoluteStr(f));
  }

  public boolean delete(Path f, boolean recursive) throws IoException {
    if (recursive)
      return m_client.redfishUnlinkTree(makeAbsoluteStr(f));
    else
      return m_client.redfishUnlink(makeAbsoluteStr(f));'
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

  public boolean mkdirs(Path path, FsPermission perm) throws IoException {
    return m_client.redfishMkdirs(makeAbsoluteStr(path), perm.toShort());
  }

  public FileStatus getFileStatus(Path path) throws IOException {
    return m_client.redfishGetPathStatus(makeAbsoluteStr(path));
  }

  public void setOwner(Path p, String user, String group) throws IOException {
    m_client.redfishChown(makeAbsoluteStr(p), user, group);
  }

  public void setPermission(Path p, FsPermission perm) throws IOException {
    m_client.redfishChmod(makeAbsoluteStr(p), perm.toShort());
  }

  public void setTimes(Path p, long mtime, long atime) throws IOException {
    m_client.redfishUtimes(makeAbsoluteStr(p), mtime, atime);
  }
};
