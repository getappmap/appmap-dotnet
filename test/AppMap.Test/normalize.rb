#!/usr/bin/env ruby

# normalize appmaps to prepare for diffing

DIR = ARGV[0] || fail("directory needed")

Dir["#{DIR}/**/*.appmap.json"].each do |file|
  text = File.read file

  thread_seq = 0
  threads = Hash.new do |h, tid|
    h[tid] = (thread_seq += 1)
  end

  text.gsub!(/"thread_id":\s*\d+/) do |thread_clause|
    thread_clause.gsub(/\d+/, threads)
  end

  File.write file, text
end
