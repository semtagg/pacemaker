.. _utilization:

Utilization and Placement Strategy
----------------------------------

Pacemaker decides where to place a resource according to the resource
allocation scores on every node. The resource will be allocated to the
node where the resource has the highest score.

If the resource allocation scores on all the nodes are equal, by the default
placement strategy, Pacemaker will choose a node with the least number of
allocated resources for balancing the load. If the number of resources on each
node is equal, the first eligible node listed in the CIB will be chosen to run
the resource.

Often, in real-world situations, different resources use significantly
different proportions of a node's capacities (memory, I/O, etc.).
We cannot balance the load ideally just according to the number of resources
allocated to a node. Besides, if resources are placed such that their combined
requirements exceed the provided capacity, they may fail to start completely or
run with degraded performance.

To take these factors into account, Pacemaker allows you to configure:

#. The capacity a certain node provides.

#. The capacity a certain resource requires.

#. An overall strategy for placement of resources.

Utilization attributes
######################

To configure the capacity that a node provides or a resource requires,
you can use *utilization attributes* in ``node`` and ``resource`` objects.
You can name utilization attributes according to your preferences and define as
many name/value pairs as your configuration needs. However, the attributes'
values must be integers.

.. topic: Specifying CPU and RAM capacities of two nodes

   .. code-block:: xml

      <node id="node1" type="normal" uname="node1">
        <utilization id="node1-utilization">
          <nvpair id="node1-utilization-cpu" name="cpu" value="2"/>
          <nvpair id="node1-utilization-memory" name="memory" value="2048"/>
        </utilization>
      </node>
      <node id="node2" type="normal" uname="node2">
        <utilization id="node2-utilization">
          <nvpair id="node2-utilization-cpu" name="cpu" value="4"/>
          <nvpair id="node2-utilization-memory" name="memory" value="4096"/>
        </utilization>
      </node>

.. topic:: Specifying CPU and RAM consumed by several resources

   .. code-block:: xml

      <primitive id="rsc-small" class="ocf" provider="pacemaker" type="Dummy">
        <utilization id="rsc-small-utilization">
          <nvpair id="rsc-small-utilization-cpu" name="cpu" value="1"/>
          <nvpair id="rsc-small-utilization-memory" name="memory" value="1024"/>
        </utilization>
      </primitive>
      <primitive id="rsc-medium" class="ocf" provider="pacemaker" type="Dummy">
        <utilization id="rsc-medium-utilization">
          <nvpair id="rsc-medium-utilization-cpu" name="cpu" value="2"/>
          <nvpair id="rsc-medium-utilization-memory" name="memory" value="2048"/>
        </utilization>
      </primitive>
      <primitive id="rsc-large" class="ocf" provider="pacemaker" type="Dummy">
        <utilization id="rsc-large-utilization">
          <nvpair id="rsc-large-utilization-cpu" name="cpu" value="3"/>
          <nvpair id="rsc-large-utilization-memory" name="memory" value="3072"/>
        </utilization>
      </primitive>

A node is considered eligible for a resource if it has sufficient free
capacity to satisfy the resource's requirements. The nature of the required
or provided capacities is completely irrelevant to Pacemaker -- it just makes
sure that all capacity requirements of a resource are satisfied before placing
a resource to a node.

.. note::

   Utilization is supported for bundles *(since 2.1.3)*, but only for bundles
   with an inner primitive. Any resource utilization values should be specified
   for the inner primitive, but any priority meta-attribute should be specified
   for the outer bundle.


Placement Strategy
##################

After you have configured the capacities your nodes provide and the
capacities your resources require, you need to set the ``placement-strategy``
in the global cluster options, otherwise the capacity configurations have
*no effect*.

Four values are available for the ``placement-strategy``: 

* **default**

   Utilization values are not taken into account at all.
   Resources are allocated according to allocation scores. If scores are equal,
   resources are evenly distributed across nodes.

* **utilization**

   Utilization values are taken into account *only* when deciding whether a node
   is considered eligible (i.e. whether it has sufficient free capacity to satisfy
   the resource's requirements). Load-balancing is still done based on the
   number of resources allocated to a node. 

* **balanced**

   Utilization values are taken into account when deciding whether a node
   is eligible to serve a resource *and* when load-balancing, so an attempt is
   made to spread the resources in a way that optimizes resource performance.

* **minimal**

   Utilization values are taken into account *only* when deciding whether a node
   is eligible to serve a resource. For load-balancing, an attempt is made to
   concentrate the resources on as few nodes as possible, thereby enabling
   possible power savings on the remaining nodes. 

Set ``placement-strategy`` with ``crm_attribute``:

   .. code-block:: none

      # crm_attribute --name placement-strategy --update balanced

Now Pacemaker will ensure the load from your resources will be distributed
evenly throughout the cluster, without the need for convoluted sets of
colocation constraints.

Allocation Details
##################

Which node is preferred to get consumed first when allocating resources?
________________________________________________________________________

* The node with the highest node weight gets consumed first. Node weight
  is a score maintained by the cluster to represent node health.

* If multiple nodes have the same node weight:

 * If ``placement-strategy`` is ``default`` or ``utilization``,
   the node that has the least number of allocated resources gets consumed first.

   * If their numbers of allocated resources are equal,
     the first eligible node listed in the CIB gets consumed first.

 * If ``placement-strategy`` is ``balanced``,
   the node that has the most free capacity gets consumed first.

   * If the free capacities of the nodes are equal,
     the node that has the least number of allocated resources gets consumed first.

     * If their numbers of allocated resources are equal,
       the first eligible node listed in the CIB gets consumed first.

 * If ``placement-strategy`` is ``minimal``,
   the first eligible node listed in the CIB gets consumed first.

Which node has more free capacity?
__________________________________

If only one type of utilization attribute has been defined, free capacity
is a simple numeric comparison.

If multiple types of utilization attributes have been defined, then
the node that is numerically highest in the the most attribute types
has the most free capacity. For example:

* If ``nodeA`` has more free ``cpus``, and ``nodeB`` has more free ``memory``,
  then their free capacities are equal.

* If ``nodeA`` has more free ``cpus``, while ``nodeB`` has more free ``memory``
  and ``storage``, then ``nodeB`` has more free capacity.

Which resource is preferred to be assigned first?
_________________________________________________

* The resource that has the highest ``priority`` (see :ref:`resource_options`) gets
  allocated first.

* If their priorities are equal, check whether they are already running. The
  resource that has the highest score on the node where it's running gets allocated
  first, to prevent resource shuffling.

* If the scores above are equal or the resources are not running, the resource has
  the highest score on the preferred node gets allocated first.

* If the scores above are equal, the first runnable resource listed in the CIB
  gets allocated first.

Limitations and Workarounds
###########################

The type of problem Pacemaker is dealing with here is known as the
`knapsack problem <http://en.wikipedia.org/wiki/Knapsack_problem>`_ and falls into
the `NP-complete <http://en.wikipedia.org/wiki/NP-complete>`_ category of computer
science problems -- a fancy way of saying "it takes a really long time
to solve".

Clearly in a HA cluster, it's not acceptable to spend minutes, let alone hours
or days, finding an optimal solution while services remain unavailable.

So instead of trying to solve the problem completely, Pacemaker uses a
*best effort* algorithm for determining which node should host a particular
service. This means it arrives at a solution much faster than traditional
linear programming algorithms, but by doing so at the price of leaving some
services stopped.

In the contrived example at the start of this chapter:

* ``rsc-small`` would be allocated to ``node1``

* ``rsc-medium`` would be allocated to ``node2``

* ``rsc-large`` would remain inactive

Which is not ideal.

There are various approaches to dealing with the limitations of
pacemaker's placement strategy:

* **Ensure you have sufficient physical capacity.**

   It might sound obvious, but if the physical capacity of your nodes is (close to)
   maxed out by the cluster under normal conditions, then failover isn't going to
   go well. Even without the utilization feature, you'll start hitting timeouts and
   getting secondary failures.

* **Build some buffer into the capabilities advertised by the nodes.**

   Advertise slightly more resources than we physically have, on the (usually valid)
   assumption that a resource will not use 100% of the configured amount of
   CPU, memory and so forth *all* the time. This practice is sometimes called *overcommit*.

* **Specify resource priorities.**

   If the cluster is going to sacrifice services, it should be the ones you care
   about (comparatively) the least. Ensure that resource priorities are properly set
   so that your most important resources are scheduled first. 
