#!/usr/bin/env ruby

PLOAD_VERSION = '1.0.0'

# This Ruby script tests that the basic functions of p-load are working.  This
# should be run on each supported operating system to test release candidates.
# It just runs the p-load found on the PATH.

Dir.chdir(File.dirname(__FILE__))

require 'rspec/expectations'

class PloadTest
  include RSpec::Matchers

  def pload(args)
    cmd = 'p-load ' + "-d #{@serial_number} " + args
    success = system(cmd)
    raise "Command failed with code #{$?}: #{cmd}" if !success
  end

  def pload_list
    output = `p-load --list`
    output.lines.to_a
  end

  def run
    # Make sure we are running the right version
    help = `p-load`
    expect(help.lines.first(2)).to eq [
      "p-load: Pololu USB Bootloader Utility\n",
      "Version #{PLOAD_VERSION}\n"
    ]

    # Make sure there is one bootloader and it is a P-Star 25K50.
    list = pload_list
    expect($?.to_i).to eq 0
    expect(list.size).to eq 1
    line = list[0]
    md = line.match(/(\A\w{8,16})\s+Pololu P-Star 25K50 Bootloader/)
    expect(md).to be
    @serial_number = md[1]

    # Write flash and EEPROM
    pload '--write test_fe.hex'

    # Verify flash and EEPROM
    pload '--read tmp.hex'
    expect(File.read('tmp.hex')).to eq File.read('test_fe.hex')

    # Restart.  The firmware should run for 5 seconds and then
    # return to the bootloader.
    pload '--restart'

    # Expect the device to be missing after 4 seconds
    sleep 4
    expect(pload_list.size).to eq 0

    # Erase
    pload '--wait --erase'

    # Verify the erased flash and EEPROM
    pload '--read tmp.hex'
    expect(File.read('tmp.hex')).to eq File.read('test_empty.hex')

    puts 'All tests passed.'
  end
end

PloadTest.new.run
